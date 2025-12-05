/**
 * @file audio_receiver.cpp
 * @brief Audio receiver implementation
 */

#include "audio_receiver.h"
#include "../../moonmic_internal.h"  // For moonmic_packet_header_t
#include "debug.h"
#include <iostream>
#include <cstring>

namespace moonmic {

AudioReceiver::AudioReceiver()
    : sunshine_(nullptr)
    , decoder_(nullptr)
    , resampler_(nullptr)
    , receiver_(nullptr)
    , virtual_device_(nullptr)
    , connection_monitor_(nullptr)
    , running_(false)
    , client_validated_(false)
    , detected_stream_rate_(0)
    , system_sample_rate_(0)  // Will be set after VirtualDevice init
    , rate_logged_(false) {
    memset(&stats_, 0, sizeof(stats_));
}

AudioReceiver::~AudioReceiver() {
    stop();
}

bool AudioReceiver::start(const Config& config) {
    if (running_) {
        return false;
    }
    
    config_ = config;
    
    // Note: Sunshine whitelist sync is not currently implemented
    // Whitelist checking would need client UUIDs sent in packets
    
    // Initialize FFmpeg Opus decoder at configured output rate
    // This will match VB-Cable's rate (can be 16kHz or 48kHz from GUI)
    decoder_ = std::make_unique<FFmpegDecoder>();
    if (!decoder_->init(config_.audio.resampling_rate, config_.audio.channels)) {
        std::cerr << "[AudioReceiver] Failed to initialize FFmpeg Opus decoder" << std::endl;
        return false;
    }
    std::cout << "[AudioReceiver] FFmpeg Opus decoder initialized at " << config_.audio.resampling_rate << "Hz" << std::endl;
    
    // Only initialize resampler if we need it (stream rate != output rate)
    // For Vita@16kHz -> VB-Cable@16kHz, no resampling needed
    // For Vita@16kHz -> VB-Cable@48kHz, use Speex resampler
    resampler_ = nullptr;  // Will be created on-demand when stream rate is detected
    
    // Initialize audio output device
    // Speaker mode: use default system speakers (empty device name)
    // Normal mode: use VB-Cable virtual microphone
    virtual_device_ = VirtualDevice::create();
    std::string output_device = config_.audio.use_speaker_mode ? "" : config_.audio.virtual_device_name;
    std::string output_mode = config_.audio.use_speaker_mode ? "speakers (debug)" : "VB-Cable (microphone)";
    
    // Initialize with a dummy rate - VirtualDevice will use system's native format
    if (!virtual_device_->init(
        output_device,
        48000,  // Dummy value - actual rate will be detected from system
        config_.audio.channels
    )) {
        std::cerr << "[AudioReceiver] Failed to initialize audio device" << std::endl;
        return false;
    }
    
    // Get the actual system sample rate that was detected
    system_sample_rate_ = virtual_device_->getSampleRate();
    std::cout << "[AudioReceiver] Audio output: " << output_mode 
              << " @ " << system_sample_rate_ << "Hz (auto-detected)" << std::endl;
    
    // Initialize UDP receiver
    receiver_ = std::make_unique<UDPReceiver>();
    receiver_->setPacketCallback([this](const uint8_t* data, size_t size, const std::string& ip) {
        onPacketReceived(data, size, ip);
    });
    
    if (!receiver_->start(config_.server.port, config_.server.bind_address)) {
        std::cerr << "[AudioReceiver] Failed to start UDP receiver" << std::endl;
        return false;
    }
    
    running_ = true;
    std::cout << "[AudioReceiver] Started successfully" << std::endl;
    return true;
}

void AudioReceiver::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (receiver_) {
        receiver_->stop();
        receiver_.reset();
    }
    
    if (virtual_device_) {
        virtual_device_->close();
        virtual_device_.reset();
    }
    
    if (resampler_) {
        speex_resampler_destroy(resampler_);
        resampler_ = nullptr;
    }
    
    // Reset stream detection so next start() creates fresh resampler
    detected_stream_rate_ = 0;
    system_sample_rate_ = 0;
    rate_logged_ = false;
    
    if (decoder_) {
        decoder_.reset();
    }
    
    if (connection_monitor_) {
        connection_monitor_->stop();
        connection_monitor_.reset();
    }
    
    std::cout << "[AudioReceiver] Stopped" << std::endl;
}

bool AudioReceiver::isClientAllowed(const std::string& ip) {
    // If whitelist is disabled, allow all
    if (!config_.security.enable_whitelist) {
        return true;
    }
    
    // Check if IP is in allowed clients list
    for (const auto& allowed_ip : config_.security.allowed_clients) {
        if (ip == allowed_ip) {
            return true;
        }
    }
    
    return false;
}

void AudioReceiver::onPacketReceived(const uint8_t* data, size_t size, const std::string& sender_ip) {
    stats_.packets_received++;
    stats_.bytes_received += size;
    stats_.last_sender_ip = sender_ip;
    stats_.is_receiving = true;
    
    // First packet must be handshake for validation
    if (!client_validated_) {
        if (!validateHandshake(data, size, sender_ip)) {
            stats_.packets_dropped++;
            return;  // DENY - invalid handshake
        }
        client_validated_ = true;
        last_validated_ip_ = sender_ip;
        
        // Start sending pings to validated client
        if (!connection_monitor_) {
            connection_monitor_ = std::make_unique<ConnectionMonitor>();
        }
        connection_monitor_->start(sender_ip, config_.server.port);
        std::cout << "[AudioReceiver] Started heartbeat monitor for " << sender_ip << std::endl;
        
        return;  // Handshake consumed, don't process as audio
    }
    
    // Parse packet header MANUALLY to match Vita's manual writing
    // The header size is MOONMIC_HEADER_SIZE (20 bytes: 4+4+8+4)
    if (size < MOONMIC_HEADER_SIZE) {
        std::cerr << "[AudioReceiver] Packet too small: " << size << " bytes (expected at least " << MOONMIC_HEADER_SIZE << " for header)" << std::endl;
        stats_.packets_dropped++;
        return;
    }

    // Read magic (bytes 0-3, little-endian)
    uint32_t magic = ((uint32_t)data[0] << 0) | ((uint32_t)data[1] << 8) | 
                     ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    
    // Read sequence (bytes 4-7, little-endian)  
    uint32_t sequence = ((uint32_t)data[4] << 0) | ((uint32_t)data[5] << 8) |
                        ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    
    // Read timestamp (bytes 8-15, little-endian)
    uint64_t timestamp = ((uint64_t)data[8] << 0) | ((uint64_t)data[9] << 8) |
                         ((uint64_t)data[10] << 16) | ((uint64_t)data[11] << 24) |
                         ((uint64_t)data[12] << 32) | ((uint64_t)data[13] << 40) |
                         ((uint64_t)data[14] << 48) | ((uint64_t)data[15] << 56);
    
    // Read sample_rate (bytes 16-19, little-endian)
    uint32_t sample_rate_field = ((uint32_t)data[16] << 0) |
                                  ((uint32_t)data[17] << 8) |
                                  ((uint32_t)data[18] << 16) |
                                  ((uint32_t)data[19] << 24);
    
    // Check for RAW mode flag (bit 31)
    bool is_raw_mode = (sample_rate_field & MOONMIC_RAW_FLAG) != 0;
    uint32_t stream_rate = sample_rate_field & ~MOONMIC_RAW_FLAG;  // Mask out RAW flag
    
    // Log first packet details
    if (stats_.packets_received == 1) {
        std::cout << "[AudioReceiver] FIRST PACKET DEBUG (manual read):" << std::endl;
        std::cout << "  Packet size: " << size << " bytes" << std::endl;
        std::cout << "  magic = 0x" << std::hex << magic << " (expected 0x" << MOONMIC_MAGIC << ")" << std::dec << std::endl;
        std::cout << "  sequence = " << sequence << std::endl;
        std::cout << "  timestamp = " << timestamp << std::endl;
        std::cout << "  sample_rate = " << stream_rate << std::endl;
        std::cout << "  raw_mode = " << (is_raw_mode ? "YES" : "NO") << std::endl;
        std::cout << "  Raw header bytes:";
        for (int i = 0; i < 20; i++) {
            std::cout << " " << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
        }
        std::cout << std::dec << std::endl << std::endl;
    }
    
    // First packet: log stream info
    if (detected_stream_rate_ == 0) {
        detected_stream_rate_ = stream_rate;
        rate_logged_ = true;
        
        std::cout << "[AudioReceiver] ═══ Stream Detected ═══" << std::endl;
        std::cout << "[AudioReceiver] Source IP: " << sender_ip << std::endl;
        std::cout << "[AudioReceiver] Stream sample rate: " << stream_rate << " Hz" << std::endl;
        std::cout << "[AudioReceiver] Output sample rate: " << system_sample_rate_ << " Hz" << std::endl;
        std::cout << "[AudioReceiver] Mode: " << (is_raw_mode ? "RAW PCM" : "Opus") << std::endl;
        
        if (stream_rate != system_sample_rate_) {
            // Create resampler for stream_rate -> system_sample_rate_
            int err = 0;
            resampler_ = speex_resampler_init(
                config_.audio.channels,
                stream_rate,           // Input rate (16kHz from Vita)
                system_sample_rate_,   // Output rate (48kHz/96kHz system)
                10,                     // Quality (0-10, 10 = best)
                &err
            );
            
            if (err != RESAMPLER_ERR_SUCCESS || !resampler_) {
                std::cerr << "[AudioReceiver] Failed to create resampler: " << err << std::endl;
                return;
            }
            
            std::cout << "[AudioReceiver] ✓ Resampler created: " << stream_rate << "Hz → " 
                      << system_sample_rate_ << "Hz (quality 10)" << std::endl;
            std::cout << "[AudioReceiver] Resampling: " << stream_rate << "Hz → " 
                      << system_sample_rate_ << "Hz (Speex resampler)" << std::endl;
        } else {
            std::cout << "[AudioReceiver] No resampling needed (rates match)" << std::endl;
        }
        std::cout << "[AudioReceiver] ═══════════════════════\n" << std::endl;
    }
    
    // Skip header
    const uint8_t* payload = data + MOONMIC_HEADER_SIZE;
    size_t payload_size = size - MOONMIC_HEADER_SIZE;
    
    float* output_buffer = decode_buffer_;
    int output_frames = 0;
    
    if (is_raw_mode) {
        // RAW mode: Convert int16 PCM to float
        const int16_t* pcm_int16 = (const int16_t*)payload;
        int num_samples = payload_size / sizeof(int16_t);
        output_frames = num_samples / config_.audio.channels;
        
        // DEBUG: Log first packet's sample values
        static bool first_raw_logged = false;
        if (!first_raw_logged) {
            first_raw_logged = true;
            std::cout << "[AudioReceiver] RAW MODE DEBUG:" << std::endl;
            std::cout << "  Payload size: " << payload_size << " bytes" << std::endl;
            std::cout << "  Num samples: " << num_samples << std::endl;
            std::cout << "  Output frames: " << output_frames << std::endl;
            std::cout << "  First 10 int16 samples:";
            for (int i = 0; i < 10 && i < num_samples; i++) {
                std::cout << " " << pcm_int16[i];
            }
            std::cout << std::endl;
        }
        
        // Convert int16 to float
        // CRITICAL: Divide by 32768.0f (not 32767.0f) for correct normalization
        for (int i = 0; i < num_samples; i++) {
            decode_buffer_[i] = (float)pcm_int16[i] / 32768.0f;
        }
        
        // Apply resampling if stream rate != system rate
        if (system_sample_rate_ != detected_stream_rate_ && resampler_) {
            spx_uint32_t in_len = output_frames;
            spx_uint32_t out_len = MAX_FRAMES;
            
            // Debug: log first resample operation
            static bool first_resample_logged = false;
            if (!first_resample_logged) {
                first_resample_logged = true;
                std::cout << "[AudioReceiver] RAW RESAMPLE: " << in_len << " frames → ";
            }
            
            int err = speex_resampler_process_float(
                resampler_,
                0,  // channel 0 (mono)
                decode_buffer_,
                &in_len,
                resample_buffer_,
                &out_len
            );
            
            if (!first_resample_logged) {
                std::cout << out_len << " frames" << std::endl;
                first_resample_logged = true;
            }
            
            if (err != RESAMPLER_ERR_SUCCESS) {
                stats_.packets_dropped++;
                std::cerr << "[AudioReceiver] RAW resampling failed: " << err << std::endl;
                return;
            }
            
            output_buffer = resample_buffer_;
            output_frames = out_len;
        }
        
        // Log periodically
        static uint64_t raw_packet_count = 0;
        if (++raw_packet_count % 100 == 1 && raw_packet_count > 1) {
            if (isDebugMode()) {
                std::cout << "[AudioReceiver] Processing RAW: packet #" << stats_.packets_received 
                          << ", " << output_frames << " frames" << std::endl;
            }
        }
    } else {
        // Opus mode: Decode compressed audio
        int decoded_frames = decoder_->decode(payload, payload_size, decode_buffer_, MAX_FRAMES);
        if (decoded_frames < 0) {
            stats_.packets_dropped++;
            std::cerr << "[AudioReceiver] Decode failed for packet from " << sender_ip << std::endl;
            return;
        }
        
        output_frames = decoded_frames;
        
        // Resample only if needed
        if (system_sample_rate_ != detected_stream_rate_ && resampler_) {
            spx_uint32_t in_len = decoded_frames;
            spx_uint32_t out_len = MAX_FRAMES;
            
            int err = speex_resampler_process_float(
                resampler_,
                0,  // channel 0 (mono)
                decode_buffer_,
                &in_len,
                resample_buffer_,
                &out_len
            );
            
            if (err != RESAMPLER_ERR_SUCCESS) {
                stats_.packets_dropped++;
                std::cerr << "[AudioReceiver] Resampling failed: " << err << std::endl;
                return;
            }
            
            output_buffer = resample_buffer_;
            output_frames = out_len;
        }
        
        // Log periodically
        static uint64_t packet_count = 0;
        if (++packet_count % 100 == 1 && packet_count > 1) {
            if (config_.audio.resampling_rate == detected_stream_rate_) {
                if (isDebugMode()) {
                    std::cout << "[AudioReceiver] Processing Opus: packet #" << stats_.packets_received 
                              << ", decoded " << output_frames << " frames" << std::endl;
                }
            } else {
                if (isDebugMode()) {
                    std::cout << "[AudioReceiver] Processing Opus: packet #" << stats_.packets_received 
                              << ", decoded=" << decoded_frames << " frames @ " << detected_stream_rate_ << "Hz"
                              << ", resampled=" << output_frames << " frames @ " << config_.audio.resampling_rate 
                              << "Hz (Speex)" << std::endl;
                }
            }
        }
    }
    
    // Send to virtual device or speakers depending on mode
    if (!virtual_device_->write(output_buffer, output_frames, config_.audio.channels)) {
        // Write failed, but don't count as dropped
    }
    
    stats_.is_receiving = true;
}


bool AudioReceiver::validateHandshake(const uint8_t* data, size_t size, const std::string& sender_ip) {
    if (size < sizeof(MoonMicHandshake)) {
        std::cerr << "[AudioReceiver] Packet too small for handshake: " << size << " bytes" << std::endl;
        return false;
    }
    
    const MoonMicHandshake* hs = reinterpret_cast<const MoonMicHandshake*>(data);
    
    // Check magic number - handle both little-endian and big-endian
    // Little-endian (PS Vita): 0x4E4F4F4D -> "MOON" in bytes
    // Big-endian: 0x4D4F4F4E -> "MOON" in bytes
    uint32_t magic = hs->magic;
    if (magic != 0x4D4F4F4E && magic != 0x4E4F4F4D) {
        std::cerr << "[AudioReceiver] Invalid handshake magic: 0x" 
                  << std::hex << magic << std::dec << std::endl;
        return false;
    }
    
    if (hs->uniqueid_len > 0 && hs->uniqueid_len <= 16) {
        client_uniqueid_ = std::string(hs->uniqueid, hs->uniqueid_len);
    }
    
    if (hs->devicename_len > 0 && hs->devicename_len <= 64) {
        client_devicename_ = std::string(hs->devicename, hs->devicename_len);
        stats_.client_name = client_devicename_;
    }
    
    if (!config_.security.enable_whitelist) {
        std::cout << "[AudioReceiver] Client connected: " << client_devicename_ << " [whitelist disabled]" << std::endl;
        return true;
    }
    
    if (hs->pair_status != 1) {
        std::cerr << "[AudioReceiver] DENY: Client not paired - " << client_devicename_ << std::endl;
        return false;
    }
    
    std::cout << "[AudioReceiver] Client validated: " << client_devicename_ << std::endl;
    return true;
}

} // namespace moonmic

