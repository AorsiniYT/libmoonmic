/**
 * @file audio_receiver.cpp
 * @brief Audio receiver implementation
 */

#include "audio_receiver.h"
#include "../../moonmic_internal.h"  // For moonmic_packet_header_t
#include <iostream>
#include <cstring>

namespace moonmic {

AudioReceiver::AudioReceiver()
    : running_(false) {
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
    
    // Initialize Sunshine integration if enabled
    if (config_.security.sync_with_sunshine) {
        sunshine_ = std::make_unique<SunshineIntegration>();
        if (!sunshine_->detectSunshine()) {
            std::cout << "[AudioReceiver] Sunshine not detected, whitelist disabled" << std::endl;
            config_.security.enable_whitelist = false;
        }
    }
    
    // Initialize Opus decoder
    // Opus can decode any rate stream to any output rate (built-in resampling)
    // Decode 16kHz stream from Vita to 48kHz output for VB-Cable
    decoder_ = std::make_unique<OpusDecoder>();
    if (!decoder_->init(config_.audio.resampling_rate, config_.audio.channels)) {
        std::cerr << "[AudioReceiver] Failed to initialize Opus decoder" << std::endl;
        return false;
    }
    std::cout << "[AudioReceiver] Opus decoder initialized: stream=" << config_.audio.stream_sample_rate 
              << "Hz -> output=" << config_.audio.resampling_rate << "Hz (Opus internal resampling)" << std::endl;
    
    // Initialize virtual audio device with output sample rate
    virtual_device_ = VirtualDevice::create();
    if (!virtual_device_->init(
        config_.audio.virtual_device_name,
        config_.audio.resampling_rate,
        config_.audio.channels
    )) {
        std::cerr << "[AudioReceiver] Failed to initialize virtual device" << std::endl;
        return false;
    }
    std::cout << "[AudioReceiver] Virtual device initialized at " << config_.audio.resampling_rate << "Hz" << std::endl;
    
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
    }
    
    if (virtual_device_) {
        virtual_device_->close();
    }
    
    std::cout << "[AudioReceiver] Stopped" << std::endl;
}

bool AudioReceiver::isClientAllowed(const std::string& ip) {
    // If whitelist is disabled, allow all
    if (!config_.security.enable_whitelist) {
        return true;
    }
    
    // TODO: Map IP to client UUID (would need client to send UUID in packet)
    // For now, just allow if Sunshine integration is working
    return sunshine_ && sunshine_->isSunshineDetected();
}

void AudioReceiver::onPacketReceived(const uint8_t* data, size_t size, const std::string& sender_ip) {
    stats_.packets_received++;
    stats_.bytes_received += size;
    stats_.last_sender_ip = sender_ip;
    stats_.is_receiving = true;
    
    // Check whitelist
    if (!isClientAllowed(sender_ip)) {
        stats_.packets_dropped++;
        return;
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
    uint32_t stream_rate = ((uint32_t)data[16] << 0) | ((uint32_t)data[17] << 8) |
                           ((uint32_t)data[18] << 16) | ((uint32_t)data[19] << 24);
    
    // DEBUG: Log first packet header
    static bool first_packet = true;
    if (first_packet) {
        first_packet = false;
        std::cout << "[AudioReceiver] FIRST PACKET DEBUG (manual read):" << std::endl;
        std::cout << "  Packet size: " << size << " bytes" << std::endl;
        std::cout << "  magic = 0x" << std::hex << magic << std::dec << " (expected 0x4D4D4943)" << std::endl;
        std::cout << "  sequence = " << sequence << std::endl;
        std::cout << "  timestamp = " << timestamp << std::endl;
        std::cout << "  sample_rate = " << stream_rate << std::endl;
        std::cout << "  Raw header bytes: ";
        for (size_t i = 0; i < std::min(size, (size_t)MOONMIC_HEADER_SIZE); i++) {
            printf("%02X ", data[i]);
        }
        std::cout << std::endl;
    }
    
    // Auto-detect and adapt to stream sample rate
    if (detected_stream_rate_ == 0) {
        detected_stream_rate_ = stream_rate;
        std::cout << "\n[AudioReceiver] ═══ Stream Detected ═══" << std::endl;
        std::cout << "[AudioReceiver] Source IP: " << sender_ip << std::endl;
        std::cout << "[AudioReceiver] Stream sample rate: " << stream_rate << " Hz" << std::endl;
        std::cout << "[AudioReceiver] Output sample rate: " << config_.audio.resampling_rate << " Hz" << std::endl;
        
        if (stream_rate != config_.audio.resampling_rate) {
            std::cout << "[AudioReceiver] Resampling: " << stream_rate << "Hz → " 
                      << config_.audio.resampling_rate << "Hz (Opus internal)" << std::endl;
        } else {
            std::cout << "[AudioReceiver] No resampling needed (rates match)" << std::endl;
        }
        std::cout << "[AudioReceiver] ═══════════════════════\n" << std::endl;
        
        // Reinitialize decoder if detected rate differs from configured
        if (stream_rate != config_.audio.stream_sample_rate) {
            std::cout << "[AudioReceiver] Auto-adjusting decoder for " << stream_rate << "Hz stream" << std::endl;
            config_.audio.stream_sample_rate = stream_rate;
            // Note: Opus decoder is already initialized for output rate (resampling_rate)
            // Opus will handle the resampling internally from stream_rate to resampling_rate
        }
    } else if (stream_rate != detected_stream_rate_ && !rate_logged_) {
        std::cout << "[AudioReceiver] WARNING: Stream rate changed: " 
                  << detected_stream_rate_ << "Hz → " << stream_rate << "Hz" << std::endl;
        detected_stream_rate_ = stream_rate;
        rate_logged_ = true;
    }
    
    // Skip header, decode Opus data
    const uint8_t* opus_data = data + MOONMIC_HEADER_SIZE;
    size_t opus_size = size - MOONMIC_HEADER_SIZE;
    
    // Decode Opus
    int frames = decoder_->decode(opus_data, opus_size, audio_buffer_, MAX_FRAMES);
    if (frames < 0) {
        stats_.packets_dropped++;
        std::cerr << "[AudioReceiver] Decode failed for packet from " << sender_ip << std::endl;
        return;
    }
    
    // Log successful resampling periodically
    static uint64_t packet_count = 0;
    if (++packet_count % 100 == 1 && packet_count > 1) {
        std::cout << "[AudioReceiver] Resampling stats: " << stats_.packets_received 
                  << " packets, " << frames << " frames/packet @ " 
                  << config_.audio.resampling_rate << "Hz" << std::endl;
    }
    
    // Write to virtual device
    if (!virtual_device_->write(audio_buffer_, frames, config_.audio.channels)) {
        // Write failed, but don't count as dropped
    }
}

} // namespace moonmic
