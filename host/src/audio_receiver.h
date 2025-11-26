/**
 * @file audio_receiver.h
 * @brief Main audio receiver coordinator
 */

#pragma once

#include "config.h"
#include "sunshine_integration.h"
#include "codec/opus_decoder.h"
#include "network/udp_receiver.h"
#include "platform/virtual_device.h"
#include <memory>
#include <string>
#include <atomic>

namespace moonmic {

class AudioReceiver {
public:
    AudioReceiver();
    ~AudioReceiver();
    
    bool start(const Config& config);
    void stop();
    bool isRunning() const { return running_; }
    
    // Stats
    struct Stats {
        uint64_t packets_received = 0;
        uint64_t packets_dropped = 0;
        uint64_t bytes_received = 0;
        std::string last_sender_ip;
        bool is_receiving = false;
    };
    
    Stats getStats() const { return stats_; }
    
private:
    void onPacketReceived(const uint8_t* data, size_t size, const std::string& sender_ip);
    bool isClientAllowed(const std::string& ip);
    
    Config config_;
    std::unique_ptr<SunshineIntegration> sunshine_;
    std::unique_ptr<OpusDecoder> decoder_;
    std::unique_ptr<UDPReceiver> receiver_;
    std::unique_ptr<VirtualDevice> virtual_device_;
    
    std::atomic<bool> running_;
    Stats stats_;
    
    // Auto-detected stream sample rate
    uint32_t detected_stream_rate_ = 0;
    bool rate_logged_ = false;
    
    // Audio buffer
    static constexpr size_t MAX_FRAMES = 5760;  // 120ms at 48kHz
    float audio_buffer_[MAX_FRAMES * 2];  // Stereo
};

} // namespace moonmic
