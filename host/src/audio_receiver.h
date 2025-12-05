/**
 * @file audio_receiver.h
 * @brief Main audio receiver coordinator
 */

#pragma once

#include "config.h"
#include "sunshine_integration.h"
#include "codec/ffmpeg_decoder.h"
#include "network/udp_receiver.h"
#include "network/connection_monitor.h"
#include "platform/virtual_device.h"
#include <speex/speex_resampler.h>
#include <memory>
#include <string>
#include <atomic>
#include <cstdint>

namespace moonmic {

// Handshake packet structure (matches libmoonmic client)
#pragma pack(push, 1)
struct MoonMicHandshake {
    uint32_t magic;           // 0x4D4F4F4E ("MOON")
    uint8_t version;          // 1
    uint8_t pair_status;      // 0 or 1 from Sunshine validation
    uint8_t uniqueid_len;     // Length of uniqueid (16)
    char uniqueid[16];        // Client uniqueid
    uint8_t devicename_len;   // Length of devicename
    char devicename[64];      // Device name
};
#pragma pack(pop)

// Forward declaration (defined in connection_monitor.h)
struct MoonMicPing;

/**
 * @brief Audio receiver with Opus/PCM support
 */
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
        std::string client_name; 
        bool is_receiving = false;
    };
    
    Stats getStats() const { return stats_; }
    
private:
    void onPacketReceived(const uint8_t* data, size_t size, const std::string& sender_ip);
    bool isClientAllowed(const std::string& ip);
    bool validateHandshake(const uint8_t* data, size_t size, const std::string& sender_ip);
    
    Config config_;
    std::unique_ptr<SunshineIntegration> sunshine_;
    std::unique_ptr<FFmpegDecoder> decoder_;
    SpeexResamplerState* resampler_;  // Speex resampler (16kHz -> 48kHz)
    std::unique_ptr<UDPReceiver> receiver_;
    std::unique_ptr<VirtualDevice> virtual_device_;
    std::unique_ptr<ConnectionMonitor> connection_monitor_;
    
    std::atomic<bool> running_;
    Stats stats_;
    
    // Handshake validation state
    bool client_validated_;
    std::string client_uniqueid_;
    std::string client_devicename_;
    std::string last_validated_ip_;  // IP of validated client
    
    // Auto-detected stream sample rate
    uint32_t detected_stream_rate_ = 0;
    int system_sample_rate_ = 0;  // Auto-detected system output rate (48k, 96k, etc)
    bool rate_logged_ = false;
    
    // Audio buffers
    static constexpr size_t MAX_FRAMES = 5760;  // 120ms at 48kHz
    float decode_buffer_[MAX_FRAMES * 2];  // Decoded audio at 16kHz
    float resample_buffer_[MAX_FRAMES * 2];  // Resampled audio at 48kHz
};

} // namespace moonmic
