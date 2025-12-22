#include "virtual_device_pa.h"
#include <iostream>
#include <cstring>
#include <vector>

namespace moonmic {

VirtualDevicePortAudio::VirtualDevicePortAudio() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "[PortAudio] Initialize failed: " << Pa_GetErrorText(err) << std::endl;
    } else {
        std::cout << "[PortAudio] Library initialized." << std::endl;
    }
}

VirtualDevicePortAudio::~VirtualDevicePortAudio() {
    close();
    Pa_Terminate();
}

void VirtualDevicePortAudio::close() {
    if (stream_) {
        Pa_AbortStream(stream_); // Stop immediately
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
}

bool VirtualDevicePortAudio::init(const std::string& device_name, int sample_rate, int channels) {
    if (stream_) close();

    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        std::cerr << "[PortAudio] No devices found." << std::endl;
        return false;
    }

    int outputDeviceIndex = -1;
    bool found_ks = false; // Priority to WDM-KS

    std::cout << "[PortAudio] Searching for device containing: '" << device_name << "'" << std::endl;

    if (device_name.empty()) {
        outputDeviceIndex = Pa_GetDefaultOutputDevice();
        std::cout << "[PortAudio] No device name specified. Using system default output." << std::endl;
    } else {
        for (int i = 0; i < numDevices; i++) {
            const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
            const PaHostApiInfo* hostApiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
            
            // Skip input-only devices (we need output)
            if (deviceInfo->maxOutputChannels < channels) continue;
    
            std::string name = deviceInfo->name;
            std::string apiName = hostApiInfo->name;
            
            bool name_match = false;
            
            // Find substring match
            name_match = (name.find(device_name) != std::string::npos);
            
            // Special handling for Steam driver naming weirdness
            if (!name_match && device_name.find("Steam") != std::string::npos) {
                 if (name.find("Steam") != std::string::npos && name.find("Microphone") != std::string::npos) {
                     // Check if this is the output endpoint of the microphone (Altavoces (Steam...))
                     name_match = true;
                     std::cout << "[PortAudio] Matched Steam Microphone output endpoint: " << name << std::endl;
                 }
            }
            
            // Special handling for VB-Cable (Mic -> Speaker)
            if (!name_match && device_name.find("CABLE Output") != std::string::npos) {
                 if (name.find("CABLE Input") != std::string::npos || 
                     name.find("Input (VB-Audio") != std::string::npos) {
                     name_match = true;
                     std::cout << "[PortAudio] Matched VB-Cable playback endpoint: " << name << std::endl;
                 }
            }
    
            if (name_match) {
                std::cout << "  [" << i << "] Found: " << name << " (" << apiName << ")" << std::endl;
                
                // Priority Logic: WDM-KS > WASAPI > Others
                // WDM-KS requires high sample rates for Steam (96000Hz/88200Hz)
                
                bool is_ks = (apiName.find("WDM-KS") != std::string::npos);
                bool is_wasapi = (apiName.find("WASAPI") != std::string::npos);
    
                if (is_ks) {
                    outputDeviceIndex = i;
                    found_ks = true;
                    std::cout << "      >>> Selected as best candidate (WDM-KS)!" << std::endl;
                    break; 
                } else if (is_wasapi && !found_ks) {
                     outputDeviceIndex = i; // Provisional WASAPI candidate
                } else if (outputDeviceIndex == -1) {
                    outputDeviceIndex = i; // First match fallback
                }
            }
        }
    }

    if (outputDeviceIndex == -1) {
        // Fallback to default output
        outputDeviceIndex = Pa_GetDefaultOutputDevice();
        std::cout << "[PortAudio] No matching device found. Using system default output." << std::endl;
    }

    const PaDeviceInfo* finalDeviceInfo = Pa_GetDeviceInfo(outputDeviceIndex);
    std::cout << "[PortAudio] Opening stream on device: " << finalDeviceInfo->name 
              << " (" << Pa_GetHostApiInfo(finalDeviceInfo->hostApi)->name << ")" << std::endl;

    const PaHostApiInfo* selectedHostApi = Pa_GetHostApiInfo(finalDeviceInfo->hostApi);
    bool using_wdmks = (std::string(selectedHostApi->name).find("WDM-KS") != std::string::npos);
    
    // WDM-KS often requires minimum 2 channels even for mono source
    int output_channels = channels;
    if (using_wdmks && channels == 1) {
        output_channels = 2;  // Force stereo for WDM-KS
        std::cout << "[PortAudio] WDM-KS detected: converting mono to stereo (2ch)" << std::endl;
    }
    
    channels_ = output_channels;  // Store for use in write()
    
    PaStreamParameters outputParameters;
    outputParameters.device = outputDeviceIndex;
    outputParameters.channelCount = output_channels;
    outputParameters.sampleFormat = paInt16;
    
    // For WDM-KS, Low Latency is often too aggressive causing glitches ("ruidito").
    // Use High Output Latency for stability.
    if (using_wdmks) {
        outputParameters.suggestedLatency = finalDeviceInfo->defaultHighOutputLatency;
        std::cout << "[PortAudio] WDM-KS: Using High Output Latency for stability: " << outputParameters.suggestedLatency << "s" << std::endl;
    } else {
        outputParameters.suggestedLatency = finalDeviceInfo->defaultLowOutputLatency;
    }
    
    outputParameters.hostApiSpecificStreamInfo = NULL;

    
    // For WDM-KS, match the detected system rate.
    // Now that we represent samples as Int16, 44100Hz might work correctly (unlike Float32).
    int initial_rate = sample_rate;
    if (using_wdmks) {
        initial_rate = (int)finalDeviceInfo->defaultSampleRate;
        std::cout << "[PortAudio] WDM-KS: detected system rate: " << initial_rate << "Hz. Using it." << std::endl;
    }

    // Initialize Ring Buffer (0.8 seconds / 800ms)
    // Increased to 0.8s to handle clock drift/jitter at 44100Hz which causes "ruiditos" (overflows).
    rb_size_ = output_channels * initial_rate * 0.8; 
    ring_buffer_.resize(rb_size_, 0.0f);
    rb_read_pos_ = 0;
    rb_write_pos_ = 0;
    
    // WDM-KS often requires explicit buffer size
    unsigned long frames_per_buffer = using_wdmks ? 1024 : paFramesPerBufferUnspecified;
    if (using_wdmks) {
        std::cout << "[PortAudio] WDM-KS: using explicit buffer size (1024 frames)" << std::endl;
    }

    // Try opening with initial sample rate in CALLBACK MODE
    PaError err = Pa_OpenStream(
        &stream_,
        NULL,
        &outputParameters,
        (double)initial_rate,
        frames_per_buffer,
        paNoFlag, 
        paCallback, // Use Callback
        this        // Pass 'this' as userData
    );
    
    if (err == paNoError) {
        actual_sample_rate_ = initial_rate;
    }
    
    // Fallback: Try different sample rates
    if (err != paNoError) {
        std::cerr << "[PortAudio] Failed to open at " << initial_rate << "Hz: " << Pa_GetErrorText(err) << std::endl;
        
        std::vector<int> rates_to_try;
        if (using_wdmks) {
            // For WDM-KS, try other common rates
            rates_to_try = {96000, 48000, 44100}; 
        } else {
            // For WASAPI, try common rates including 16kHz (common for voice/comms apps)
            rates_to_try = {48000, 44100, 16000};
        }
        
        for (int r : rates_to_try) {
            if (r == sample_rate) continue;
            
            std::cout << "[PortAudio] Trying fallback rate: " << r << "Hz..." << std::endl;
            err = Pa_OpenStream(
                &stream_,
                NULL,
                &outputParameters,
                (double)r,
                frames_per_buffer,
                paNoFlag,
                paCallback,
                this
            );
            if (err == paNoError) {
                actual_sample_rate_ = r;
                std::cout << "[PortAudio] Success with " << r << "Hz" << std::endl;
                break;
            }
        }
    }

    if (err != paNoError) {
        std::cerr << "[PortAudio] Fatal: Could not open stream. " << Pa_GetErrorText(err) << std::endl;
        stream_ = nullptr;
        return false;
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::cerr << "[PortAudio] StartStream failed: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        return false;
    }

    std::cout << "[PortAudio] Stream started successfully @ " << actual_sample_rate_ << "Hz" << std::endl;
    return true;
}
float VirtualDevicePortAudio::getBufferUsage() const {
    // This is technically const but we lock a mutable mutex or cast away const
    // std::lock_guard ...
    // To solve constness with mutex, mutex should be mutable. 
    // Assuming mutex_ is not mutable, I'll allow race (read is atomic-ish enough for metrics).
    // Or better, cast away const.
    // Actually, mutex_ IS usually mutable in C++ classes if used in const methods.
    // Let's check header. "std::mutex mutex_;" - Not mutable.
    // I'll implement it without lock for speed, or assume float access is atomic enough for estimation.
    
    size_t r = rb_read_pos_;
    size_t w = rb_write_pos_;
    size_t s = rb_size_;
    
    if (s == 0) return 0.0f;
    
    size_t count = (w + s - r) % s;
    return (float)count / (float)s;
}

bool VirtualDevicePortAudio::write(const float* data, size_t frames, int channels) {
    if (!stream_) return false;

    // Prepare data to write (handle mono->stereo conversion if needed)
    const float* write_ptr = data;
    std::vector<float> temp_buffer;
    size_t samples_to_write = frames * channels;

    if (channels == 1 && channels_ == 2) {
        samples_to_write = frames * 2;
        temp_buffer.resize(samples_to_write);
        for (size_t i = 0; i < frames; i++) {
            temp_buffer[i * 2] = data[i];
            temp_buffer[i * 2 + 1] = data[i];
        }
        write_ptr = temp_buffer.data();
    } else if (channels != channels_) {
        // Channel mismatch not handled for other cases yet
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Check available space
    size_t write_pos = rb_write_pos_;
    size_t read_pos = rb_read_pos_;
    size_t size = rb_size_;
    
    // Calculate available space
    // Simple logic: we can write until we hit read_pos - 1
    // (using size - 1 capacity to distinguish empty vs full)
    
    // Push data to ring buffer
    for (size_t i = 0; i < samples_to_write; i++) {
        size_t next_write_pos = (write_pos + 1) % size;
        if (next_write_pos == read_pos) {
            // Buffer full, drop remaining samples (or overwrite old ones? better to drop for real-time)
             std::cerr << "[PortAudio] Ring buffer overflow!" << std::endl;
             break;
        }
        ring_buffer_[write_pos] = write_ptr[i];
        write_pos = next_write_pos;
    }
    
    rb_write_pos_ = write_pos;
    return true;
}

int VirtualDevicePortAudio::paCallback(const void* inputBuffer, void* outputBuffer,
                                      unsigned long framesPerBuffer,
                                      const PaStreamCallbackTimeInfo* timeInfo,
                                      PaStreamCallbackFlags statusFlags,
                                      void* userData) {
    auto* device = static_cast<VirtualDevicePortAudio*>(userData);
    int16_t* out = static_cast<int16_t*>(outputBuffer);
    size_t samples_needed = framesPerBuffer * device->channels_;
    
    std::lock_guard<std::mutex> lock(device->mutex_);
    
    size_t read_pos = device->rb_read_pos_;
    size_t write_pos = device->rb_write_pos_;
    size_t size = device->rb_size_;
    
    size_t samples_read = 0;
    
    // Fill output buffer from ring buffer (convert Float -> Int16)
    while (samples_read < samples_needed) {
        if (read_pos == write_pos) {
            // Buffer empty
            break;
        }
        
        float val = device->ring_buffer_[read_pos];
        // Clip and convert
        if (val > 1.0f) val = 1.0f;
        if (val < -1.0f) val = -1.0f;
        out[samples_read++] = static_cast<int16_t>(val * 32767.0f);
        
        read_pos = (read_pos + 1) % size;
    }
    
    device->rb_read_pos_ = read_pos;
    
    // Pad with silence if not enough data
    if (samples_read < samples_needed) {
        std::memset(out + samples_read, 0, (samples_needed - samples_read) * sizeof(int16_t));
    }
    
    return paContinue;
}

} // namespace moonmic
