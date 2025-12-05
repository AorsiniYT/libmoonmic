/**
 * @file virtual_device_win.cpp
 * @brief Windows virtual audio device using VB-CABLE
 */


#include "../virtual_device.h"
#include "driver_installer.h"
#define INITGUID  // Define GUIDs in this compilation unit
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>  // For WAVEFORMATEXTENSIBLE
#include <ksmedia.h>  // For SPEAKER_FRONT_LEFT, etc
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>

namespace moonmic {

class VirtualDeviceWindows : public VirtualDevice {
public:
    VirtualDeviceWindows() 
        : audio_client_(nullptr)
        , render_client_(nullptr)
        , buffer_frame_count_(0)
        , system_sample_rate_(0)
        , system_channels_(0) {
        // Initialize COM - ignore RPC_E_CHANGED_MODE if already initialized with different mode
        HRESULT hr = CoInitialize(NULL);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            std::cerr << "[VirtualDevice] Failed to initialize COM: 0x" << std::hex << hr << std::endl;
        }
    }
    
    ~VirtualDeviceWindows() override {
        close();
        CoUninitialize();
    }
    
    bool init(const std::string& device_name, int sample_rate, int channels) override {
        HRESULT hr;
        IMMDevice* device = nullptr;
        
        if (device_name.empty()) {
            // Speaker mode: use default system audio output device
            std::cout << "[VirtualDevice] Using default system speakers (debug mode)" << std::endl;
            
            IMMDeviceEnumerator* enumerator = NULL;
            hr = CoCreateInstance(
                __uuidof(MMDeviceEnumerator),
                NULL,
                CLSCTX_ALL,
                __uuidof(IMMDeviceEnumerator),
                (void**)&enumerator
            );
            
            if (FAILED(hr)) {
                std::cerr << "[VirtualDevice] Failed to create device enumerator" << std::endl;
                return false;
            }
            
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
            enumerator->Release();
            
            if (FAILED(hr) || !device) {
                std::cerr << "[VirtualDevice] Failed to get default audio device" << std::endl;
                return false;
            }
            
            std::cout << "[VirtualDevice] Initialized with default speakers" << std::endl;
        } else {
            // Normal mode: use VB-CABLE virtual microphone
            DriverInstaller installer;
            std::string vb_cable_input = installer.getVBCableInputDevice();
            
            if (vb_cable_input.empty()) {
                std::cerr << "[VirtualDevice] VB-CABLE not detected" << std::endl;
                std::cerr << "[VirtualDevice] Please install VB-CABLE from driver/ folder" << std::endl;
                std::cerr << "[VirtualDevice] Or run: moonmic-host --install-driver" << std::endl;
                return false;
            }
            
            std::cout << "[VirtualDevice] Found VB-CABLE: " << vb_cable_input << std::endl;
            
            // Get VB-CABLE Input device
            device = findDeviceByName(vb_cable_input);
            if (!device) {
                std::cerr << "[VirtualDevice] Failed to open VB-CABLE Input" << std::endl;
                return false;
            }
            
            std::cout << "[VirtualDevice] Initialized with VB-CABLE Input" << std::endl;
            std::cout << "[VirtualDevice] Virtual microphone: CABLE Output" << std::endl;
        }
        
        // Activate audio client
        hr = device->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            NULL,
            (void**)&audio_client_
        );
        device->Release();
        
        if (FAILED(hr)) {
            std::cerr << "[VirtualDevice] Failed to activate audio client" << std::endl;
            return false;
        }
        
        
        // Get the system's native mix format
        WAVEFORMATEX* mix_format = nullptr;
        hr = audio_client_->GetMixFormat(&mix_format);
        if (FAILED(hr)) {
            audio_client_->Release();
            audio_client_ = nullptr;
            return false;
        }
        
        WAVEFORMATEX* target_format = nullptr;
        bool use_stereo = false;
        
        // Check if system format is EXTENSIBLE
        if (mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mix_format->cbSize >= 22) {
            WAVEFORMATEXTENSIBLE* ext_format = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix_format);
            
            // Try to create a stereo version of the extensible format
            WAVEFORMATEXTENSIBLE stereo_ext = *ext_format;
            stereo_ext.Format.nChannels = 2;
            stereo_ext.Format.nBlockAlign = (stereo_ext.Format.nChannels * stereo_ext.Format.wBitsPerSample) / 8;
            stereo_ext.Format.nAvgBytesPerSec = stereo_ext.Format.nSamplesPerSec * stereo_ext.Format.nBlockAlign;
            stereo_ext.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
            
            WAVEFORMATEX* closest_match = nullptr;
            hr = audio_client_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, 
                                                   &stereo_ext.Format, &closest_match);
            
            if (hr == S_OK) {
                // Stereo EXTENSIBLE is supported
                std::cout << "[VirtualDevice] Stereo EXTENSIBLE format supported" << std::endl;
                target_format = reinterpret_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE)));
                memcpy(target_format, &stereo_ext, sizeof(WAVEFORMATEXTENSIBLE));
                use_stereo = true;
                system_sample_rate_ = stereo_ext.Format.nSamplesPerSec;
                system_channels_ = 2;
            } else if (hr == S_FALSE && closest_match) {
                // Use the closest match suggested by Windows
                std::cout << "[VirtualDevice] Using closest match format" << std::endl;
                target_format = closest_match;
                system_sample_rate_ = closest_match->nSamplesPerSec;
                system_channels_ = closest_match->nChannels;
            }
            
            if (hr != S_OK && hr != S_FALSE) {
                if (closest_match) CoTaskMemFree(closest_match);
            }
        }
        
        // Fallback to system mix format if stereo not supported
        if (!target_format) {
            std::cout << "[VirtualDevice] Using system mix format" << std::endl;
            target_format = mix_format;
            system_sample_rate_ = mix_format->nSamplesPerSec;
            system_channels_ = mix_format->nChannels;
        }
        
        std::cout << "[VirtualDevice] Target format: " << system_sample_rate_ << "Hz, " 
                  << system_channels_ << "ch" << std::endl;
        
        // Initialize with chosen format
        hr = audio_client_->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0,
            10000000,  // 1 second buffer
            0,
            target_format,
            NULL
        );
        
        // Free allocated formats
        if (use_stereo && target_format != mix_format) {
            CoTaskMemFree(target_format);
        }
        CoTaskMemFree(mix_format);
        
        if (FAILED(hr)) {
            std::cerr << "[VirtualDevice] Failed to initialize (error: 0x" 
                      << std::hex << hr << ")" << std::endl;
            audio_client_->Release();
            audio_client_ = nullptr;
            return false;
        }
        
        std::cout << "[VirtualDevice] Successfully initialized @ " << system_sample_rate_ << "Hz, " 
                  << system_channels_ << "ch" << std::endl;
        
        // Get buffer size
        hr = audio_client_->GetBufferSize(&buffer_frame_count_);
        if (FAILED(hr)) {
            audio_client_->Release();
            audio_client_ = nullptr;
            return false;
        }
        
        // Get render client
        hr = audio_client_->GetService(
            __uuidof(IAudioRenderClient),
            (void**)&render_client_
        );
        
        if (FAILED(hr)) {
            audio_client_->Release();
            audio_client_ = nullptr;
            return false;
        }
        
        // Start audio client
        hr = audio_client_->Start();
        if (FAILED(hr)) {
            render_client_->Release();
            audio_client_->Release();
            render_client_ = nullptr;
            audio_client_ = nullptr;
            return false;
        }
        
        return true;
    }
    
    bool write(const float* data, size_t frames, int channels) override {
        if (!render_client_) {
            return false;
        }
        
        UINT32 padding;
        HRESULT hr = audio_client_->GetCurrentPadding(&padding);
        if (FAILED(hr)) {
            return false;
        }
        
        UINT32 available = buffer_frame_count_ - padding;
        if (available < frames) {
            frames = available;
        }
        
        if (frames == 0) {
            return true;
        }
        
        BYTE* buffer;
        hr = render_client_->GetBuffer(frames, &buffer);
        if (FAILED(hr)) {
            return false;
        }
        
        float* float_buffer = reinterpret_cast<float*>(buffer);
        
        // If input channels match system channels, direct copy
        if (channels == system_channels_) {
            memcpy(buffer, data, frames * channels * sizeof(float));
        } else if (channels == 1 && system_channels_ == 2) {
            // Mono to stereo: duplicate to both L/R
            for (size_t f = 0; f < frames; f++) {
                float_buffer[f * 2 + 0] = data[f];  // Left
                float_buffer[f * 2 + 1] = data[f];  // Right
            }
        } else if (channels == 1 && system_channels_ > 2) {
            // Mono to multichannel (e.g., 8ch): only fill front L/R, silence others
            for (size_t f = 0; f < frames; f++) {
                float_buffer[f * system_channels_ + 0] = data[f];  // Front Left
                float_buffer[f * system_channels_ + 1] = data[f];  // Front Right
                // Silence all other channels
                for (int ch = 2; ch < system_channels_; ch++) {
                    float_buffer[f * system_channels_ + ch] = 0.0f;
                }
            }
        } else {
            // Generic upmix: cycle through input channels
            for (size_t f = 0; f < frames; f++) {
                for (int ch = 0; ch < system_channels_; ch++) {
                    float_buffer[f * system_channels_ + ch] = data[f * channels + (ch % channels)];
                }
            }
        }
        
        hr = render_client_->ReleaseBuffer(frames, 0);
        return SUCCEEDED(hr);
    }
    
    void close() override {
        if (audio_client_) {
            audio_client_->Stop();
        }
        
        if (render_client_) {
            render_client_->Release();
            render_client_ = nullptr;
        }
        
        if (audio_client_) {
            audio_client_->Release();
            audio_client_ = nullptr;
        }
    }
    
    int getSampleRate() const override {
        return system_sample_rate_;
    }
    
private:
    IAudioClient* audio_client_;
    IAudioRenderClient* render_client_;
    UINT32 buffer_frame_count_;
    int system_sample_rate_;  // Actual system sample rate detected
    int system_channels_;      // Actual system channel count
    
    IMMDevice* findDeviceByName(const std::string& target_name) {
        IMMDeviceEnumerator* enumerator = NULL;
        IMMDeviceCollection* collection = NULL;
        
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            NULL,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            (void**)&enumerator
        );
        
        if (FAILED(hr)) {
            std::cerr << "[VirtualDevice] Failed to create device enumerator" << std::endl;
            return NULL;
        }
        
        hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
        if (FAILED(hr)) {
            std::cerr << "[VirtualDevice] Failed to enumerate audio endpoints" << std::endl;
            enumerator->Release();
            return NULL;
        }
        
        UINT count;
        collection->GetCount(&count);
        
        std::cout << "[VirtualDevice] Searching for device: '" << target_name << "'" << std::endl;
        std::cout << "[VirtualDevice] Found " << count << " render devices" << std::endl;
        
        IMMDevice* result = NULL;
        
        for (UINT i = 0; i < count; i++) {
            IMMDevice* device = NULL;
            collection->Item(i, &device);
            
            if (device) {
                IPropertyStore* props = NULL;
                device->OpenPropertyStore(STGM_READ, &props);
                
                if (props) {
                    PROPVARIANT var_name;
                    PropVariantInit(&var_name);
                    
                    props->GetValue(PKEY_Device_FriendlyName, &var_name);
                    
                    if (var_name.vt == VT_LPWSTR) {
                        std::wstring wname(var_name.pwszVal);
                        std::string name(wname.begin(), wname.end());
                        
                        std::cout << "[VirtualDevice]   Device " << i << ": '" << name << "'" << std::endl;
                        
                        // Try exact match first, then substring match
                        if (name == target_name || name.find(target_name) != std::string::npos) {
                            std::cout << "[VirtualDevice] MATCH! Using device: '" << name << "'" << std::endl;
                            result = device;
                            result->AddRef();
                        }
                    }
                    
                    PropVariantClear(&var_name);
                    props->Release();
                }
                
                device->Release();
            }
            
            if (result) break;
        }
        
        if (!result) {
            std::cerr << "[VirtualDevice] No matching device found for: '" << target_name << "'" << std::endl;
        }
        
        collection->Release();
        enumerator->Release();
        
        return result;
    }
};

std::unique_ptr<VirtualDevice> VirtualDevice::create() {
    return std::make_unique<VirtualDeviceWindows>();
}

} // namespace moonmic
