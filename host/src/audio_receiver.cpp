/**
 * @file audio_receiver.cpp
 * @brief Audio receiver implementation
 */

#include "audio_receiver.h"
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
    decoder_ = std::make_unique<OpusDecoder>();
    if (!decoder_->init(config_.audio.sample_rate, config_.audio.channels)) {
        std::cerr << "[AudioReceiver] Failed to initialize Opus decoder" << std::endl;
        return false;
    }
    
    // Initialize virtual audio device
    virtual_device_ = VirtualDevice::create();
    if (!virtual_device_->init(
        config_.audio.virtual_device_name,
        config_.audio.sample_rate,
        config_.audio.channels
    )) {
        std::cerr << "[AudioReceiver] Failed to initialize virtual device" << std::endl;
        return false;
    }
    
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
    
    // Decode Opus
    int frames = decoder_->decode(data, size, audio_buffer_, MAX_FRAMES);
    if (frames < 0) {
        stats_.packets_dropped++;
        return;
    }
    
    // Write to virtual device
    if (!virtual_device_->write(audio_buffer_, frames, config_.audio.channels)) {
        // Write failed, but don't count as dropped
    }
}

} // namespace moonmic
