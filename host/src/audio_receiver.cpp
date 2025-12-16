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
    , paused_(false)
    , client_validated_(false)
    , detected_stream_rate_(0)
    , system_sample_rate_(0)  // Will be set after VirtualDevice init
    , rate_logged_(false) {
    memset(&stats_, 0, sizeof(stats_));
}

AudioReceiver::~AudioReceiver() {
    stop();
}

void AudioReceiver::resetConnectionState() {
    client_validated_ = false;
    stats_.is_connected = false;
    stats_.is_receiving = false;
    last_validated_ip_.clear();
    last_validated_time_ = std::chrono::steady_clock::time_point{};
    if (connection_monitor_) {
        connection_monitor_->stop();
    }
    
    // Reset Audio Output to clear accumulated latency buffers
    // Completely recreate the device to ensure clean state (fixes Speaker Mode init issues)
    if (virtual_device_) {
        // std::cout << "[AudioReceiver] Resetting audio device..." << std::endl;
        virtual_device_->close();
        virtual_device_.reset(); // Destroy old instance
        
        virtual_device_ = VirtualDevice::create(); // Create fresh instance
        
        std::string output_device = config_.audio.use_speaker_mode ? "" : config_.audio.virtual_device_name;
        
        // Re-init with same params
        if (!virtual_device_->init(output_device, 48000, config_.audio.channels)) {
            std::cerr << "[AudioReceiver] Failed to recreate virtual device on reset" << std::endl;
        } else {
             system_sample_rate_ = virtual_device_->getSampleRate();
             std::cout << "[AudioReceiver] Audio device reset. Rate: " << system_sample_rate_ << "Hz" << std::endl;
        }
    }
    
    // Reset Resampler - destroy it forces re-creation on next packet with correct rates
    if (resampler_) {
        speex_resampler_destroy(resampler_);
        resampler_ = nullptr;
    }
    detected_stream_rate_ = 0;
    rate_logged_ = false;
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
    receiver_->setPacketCallback([this](const uint8_t* data, size_t size, const std::string& ip, uint16_t port, bool is_lagging) {
        onPacketReceived(data, size, ip, port, is_lagging);
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

void AudioReceiver::pause() {
    if (!running_ || paused_) return;
    
    paused_ = true;
    stats_.is_paused = true;
    
    // Send STOP signal to client
    sendControlSignal(MOONMIC_CTRL_STOP);
    
    std::cout << "[AudioReceiver] Paused - sent STOP to client" << std::endl;
}

void AudioReceiver::resume() {
    if (!running_ || !paused_) return;
    
    paused_ = false;
    stats_.is_paused = false;
    
    // Reset packet timeout to prevent immediate disconnection
    // When resuming, client needs time to send first packet
    last_packet_time_ = std::chrono::steady_clock::now();
    
    // Send START signal to client
    sendControlSignal(MOONMIC_CTRL_START);
    
    std::cout << "[AudioReceiver] Resumed - sent START to client" << std::endl;
}

void AudioReceiver::sendControlSignal(uint32_t signal_magic) {
    if (!connection_monitor_ || last_validated_ip_.empty()) {
        std::cerr << "[AudioReceiver] Cannot send control signal: no validated client" << std::endl;
        return;
    }
    
    if (!connection_monitor_->isRunning()) {
        std::cerr << "[AudioReceiver] Cannot send control signal: connection monitor not running" << std::endl;
        return;
    }
    
    // Create control packet
    moonmic_control_packet_t packet;
    packet.magic = signal_magic;
    packet.reserved = 0;
    
    // Send to client using connection monitor's socket
    connection_monitor_->sendPacket(&packet, sizeof(packet));
    
    const char* signal_name = (signal_magic == MOONMIC_CTRL_STOP) ? "STOP" : 
                              (signal_magic == MOONMIC_CTRL_START) ? "START" : "UNKNOWN";
    
    std::cout << "[AudioReceiver] Sent control signal: " << signal_name 
              << " to " << last_validated_ip_ << std::endl;
}

bool AudioReceiver::switchAudioOutput(bool use_speakers) {
    if (!running_) return false;
    
    std::cout << "[AudioReceiver] Hot-swapping audio to " 
              << (use_speakers ? "speakers" : "VB-Cable") << std::endl;
    
    // Pause briefly during switch
    bool was_paused = paused_;
    if (!was_paused) pause();
    
    // Close current virtual device
    if (virtual_device_) {
        virtual_device_->close();
        virtual_device_.reset();
    }
    
    // Destroy old resampler (new rates may be needed)
    if (resampler_) {
        speex_resampler_destroy(resampler_);
        resampler_ = nullptr;
    }
    detected_stream_rate_ = 0;
    rate_logged_ = false;
    
    // Update config
    config_.audio.use_speaker_mode = use_speakers;
    
    // Create new virtual device using factory
    std::string output_device = use_speakers ? "" : config_.audio.virtual_device_name;
    std::string output_mode = use_speakers ? "speakers (debug)" : "VB-Cable (microphone)";
    
    virtual_device_ = VirtualDevice::create();
    if (!virtual_device_->init(output_device, 48000, config_.audio.channels)) {
        std::cerr << "[AudioReceiver] Failed to initialize new audio device" << std::endl;
        if (!was_paused) resume();
        return false;
    }
    
    system_sample_rate_ = virtual_device_->getSampleRate();
    std::cout << "[AudioReceiver] Audio output: " << output_mode 
              << " @ " << system_sample_rate_ << "Hz" << std::endl;
    
    // Resume if we weren't paused before
    if (!was_paused) resume();
    
    return true;
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

void AudioReceiver::onPacketReceived(const uint8_t* data, size_t size, const std::string& sender_ip, uint16_t sender_port, bool is_lagging) {
    stats_.packets_received++;
    stats_.bytes_received += size;
    stats_.last_sender_ip = sender_ip;
    stats_.is_receiving = true;
    last_packet_time_ = std::chrono::steady_clock::now();  // Update timestamp for timeout detection

    // Handshake handling: if we receive a MOON handshake at any time, treat it as (re)connection
    const bool is_handshake_magic = (size >= sizeof(MoonMicHandshake)) &&
        (((const MoonMicHandshake*)data)->magic == 0x4D4F4F4E || ((const MoonMicHandshake*)data)->magic == 0x4E4F4F4D);

    if (is_handshake_magic) {
        // Reset state to allow new session (e.g., after client reconnect or app close)
        resetConnectionState();

        uint16_t current_w = 0, current_h = 0;
        if (!validateHandshake(data, size, sender_ip, current_w, current_h)) {
            stats_.packets_dropped++;
            return;  // DENY - invalid handshake
        }
        
        // Handshake validated successfully
        client_validated_ = true;
        last_validated_ip_ = sender_ip;
        stats_.is_connected = true;
        last_validated_time_ = std::chrono::steady_clock::now();

        if (!connection_monitor_) {
            connection_monitor_ = std::make_unique<ConnectionMonitor>();
        }
        // IMPORTANT: Use sender_port, not config_.server.port
        connection_monitor_->start(sender_ip, sender_port);
        std::cout << "[AudioReceiver] Started heartbeat monitor for " << sender_ip << ":" << sender_port << std::endl;

        // Send Handshake ACK to confirm availability to client
        // IMPORTANT: Send back the FULL packet received, not just sizeof(MoonMicHandshake)
        // Because the client's moonmic_handshake_t is larger than our local definition
        uint8_t ack_buffer[256];
        memcpy(ack_buffer, data, std::min(size, sizeof(ack_buffer)));
        
        // Modify magic to ACK and update resolution fields
        MoonMicHandshake* ack = (MoonMicHandshake*)ack_buffer;
        ack->magic = 0x4B434148; // "HACK"
        if (current_w > 0 && current_h > 0) {
            ack->display_width = current_w;
            ack->display_height = current_h;
        }
        
        // Send FULL packet back (same size as received)
        connection_monitor_->sendPacket(ack_buffer, size);
        std::cout << "[AudioReceiver] Sent Handshake ACK (" << size << " bytes) to " << sender_ip << std::endl;

        return;  // Handshake consumed, don't process as audio
    }
    
    // Check for PING (Client Latency Request)
    // Magic: 0x50494E47 ("PING")
    const uint32_t PACKET_MAGIC_PING = 0x50494E47;
    // Magic: 0x504F4E47 ("PONG")
    const uint32_t PACKET_MAGIC_PONG = 0x504F4E47;
    
    if (size >= 12) { // 4 bytes Magic + 8 bytes Timestamp
        uint32_t magic;
        memcpy(&magic, data, 4);
        
        if (magic == PACKET_MAGIC_PING) {
             // Client sent PING. Echo back as PONG for Client RTT calc.
             // Use main receiver socket to reply (better for NAT/Firewal)
             if (receiver_) {
                 // Create PONG packet with same timestamp
                 std::vector<uint8_t> pong(size);
                 memcpy(pong.data(), data, size);
                 uint32_t pong_magic = PACKET_MAGIC_PONG;
                 memcpy(pong.data(), &pong_magic, 4); // Overwrite Magic
                 
                 receiver_->sendTo(pong.data(), size, sender_ip, sender_port);
             }
             return; 
        } else if (magic == PACKET_MAGIC_PONG) {
             // Client replied PONG to our PING. Calculate Host RTT.
             uint64_t timestamp;
             memcpy(&timestamp, data + 4, 8);
             
             // Get current time in same format (system_clock micros)
             auto now = std::chrono::system_clock::now();
             auto duration = now.time_since_epoch();
             uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
             
             int64_t diff_us = (int64_t)(now_us - timestamp);
             
             // Sanity check (RTT < 5 seconds)
             if (diff_us >= 0 && diff_us < 5000000) {
                 stats_.rtt_ms = (int)(diff_us / 1000);
             }
             
             // Refresh connection alive status
             stats_.last_sender_ip = sender_ip; 
             stats_.is_receiving = true;
             last_packet_time_ = std::chrono::steady_clock::now();

             return;
        }
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
    
    // Validate magic - must be MMIC for audio packets
    // (MOON=0x4D4F4F4E is handshake, MMIC=0x4D4D4943 is audio)
    if (magic != MOONMIC_MAGIC) {
        // Not an audio packet, ignore (could be handshake probe)
        return;
    }

    // AUTO-CORRECTION: If we are lagging (backlog detected), drop audio packets to catch up
    if (is_lagging) {
        static int lag_drop_counter = 0;
        lag_drop_counter++;
        if (lag_drop_counter % 50 == 0) { // Log every 50th drop to avoid spam
             std::cout << "[AudioReceiver] ⚠ LAG DETECTED: Dropping packet to drain buffer (Backlog > 2048 bytes)" << std::endl;
        }
        stats_.packets_dropped++;
        stats_.packets_dropped_lag++; // Count specific auto-correction drops
        return; // Drop packet
    }
    
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


bool AudioReceiver::validateHandshake(const uint8_t* data, size_t size, const std::string& sender_ip, uint16_t& out_w, uint16_t& out_h) {
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

        // Reset packet timeout baseline when a fresh handshake arrives
        last_packet_time_ = std::chrono::steady_clock::now();
        stats_.is_connected = true;
        return true;
    }
    
    // =========================================================================
    // WHITELIST VALIDATION - Using pair_status from client handshake
    // =========================================================================
    // 
    // NOTE: UUID verification is NOT possible because Sunshine generates a
    // random UUID for each paired client during the pairing process, ignoring
    // the uniqueid that Moonlight clients send (e.g., "0123456789ABCDEF").
    // 
    // See Sunshine source: nvhttp.cpp line 274:
    //   add_authorized_client() { named_cert.uuid = uuid_util::uuid_t::generate().string(); }
    // 
    // This means:
    //   - Client sends uniqueid="0123456789ABCDEF" during pairing
    //   - Sunshine stores a DIFFERENT random UUID (e.g., "A1B2C3D4-E5F6-...")
    //   - The /api/clients/list endpoint returns Sunshine's generated UUID
    //   - UUIDs will NEVER match
    //
    // SECURITY: The pair_status is validated by the CLIENT against Sunshine
    // via HTTPS with TLS certificate verification BEFORE sending the handshake.
    // If PairStatus=1, it means Sunshine confirmed the client is paired.
    // =========================================================================
    
    if (hs->pair_status != 1) {
        auto now = std::chrono::steady_clock::now();
        auto grace_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_validated_time_).count();
        bool grace = (last_validated_time_.time_since_epoch().count() != 0) &&
                     (last_validated_ip_ == sender_ip) &&
                     (grace_ms < 8000);

        if (grace) {
            std::cout << "[AudioReceiver] Grace-accept pair_status=0 during Sunshine restart (" << grace_ms << "ms since last validation)" << std::endl;
        } else {
            std::cerr << "[AudioReceiver] DENY: Client '" << client_devicename_ 
                      << "' not validated by Sunshine (pair_status=" << (int)hs->pair_status << ")" << std::endl;
            std::cerr << "[AudioReceiver] Ensure client is paired with Sunshine host" << std::endl;
            return false;
        }
    }
    
    std::cout << "[AudioReceiver] Client validated (pair_status=1): " << client_devicename_ << std::endl;
    
    // =========================================================================
    // DISPLAY RESOLUTION CONFIGURATION (Protocol v2+)
    // =========================================================================
    // If client sent display resolution (version >= 2), configure Sunshine
    if (hs->version >= 2 && hs->display_width > 0 && hs->display_height > 0) {
        std::cout << "[AudioReceiver] Client requests display resolution: " 
                  << hs->display_width << "x" << hs->display_height << std::endl;
        
        // Query current resolution
        out_w = 0; out_h = 0;
        if (sunshine_webui_) {
            sunshine_webui_->getCurrentResolution(out_w, out_h);
        }

        bool force_update = (hs->flags & 0x01); // FORCE_UPDATE
        bool should_update = true;

        if (out_w > 0 && out_h > 0 && !force_update) {
            if (out_w != hs->display_width || out_h != hs->display_height) {
                std::cout << "[AudioReceiver] Resolution mismatch (Current: " << out_w << "x" << out_h 
                          << ", Target: " << hs->display_width << "x" << hs->display_height 
                          << "). Waiting for FORCE flag." << std::endl;
                should_update = false;
            }
        }

        // Validate it's a standard resolution
        bool is_valid = false;
        if (hs->display_width == 1280 && hs->display_height == 720) is_valid = true;    // 720p
        if (hs->display_width == 1600 && hs->display_height == 900) is_valid = true;    // 900p
        if (hs->display_width == 1920 && hs->display_height == 1080) is_valid = true;   // 1080p
        if (hs->display_width == 2560 && hs->display_height == 1440) is_valid = true;   // 1440p
        if (hs->display_width == 3840 && hs->display_height == 2160) is_valid = true;   // 4K
        
        if (is_valid && should_update) {
            if (!applyDisplayResolution(hs->display_width, hs->display_height)) {
                std::cerr << "[AudioReceiver] Warning: host resolution request could not be applied automatically" << std::endl;
            }
        } else if (!is_valid) {
            std::cerr << "[AudioReceiver] Invalid resolution request: " 
                      << hs->display_width << "x" << hs->display_height << std::endl;
        }
    }
    
    return true;
}

bool AudioReceiver::applyDisplayResolution(uint16_t width, uint16_t height) {
    bool applied = false;
    bool attempted_sunshine = false;
    
    if (sunshine_webui_) {
        attempted_sunshine = true;
        
        // Check if resolution already matches to avoid unnecessary restart
        uint16_t current_w = 0, current_h = 0;
        bool has_current = sunshine_webui_->getCurrentResolution(current_w, current_h);
        bool already_correct = has_current && (current_w == width) && (current_h == height);
        
        if (already_correct) {
            std::cout << "[AudioReceiver] Resolution already set to " << width << "x" << height 
                      << " - No restart needed" << std::endl;
            return true;
        }
        
        if (sunshine_webui_->setDisplayResolution(width, height)) {
            std::cout << "[AudioReceiver] ✓ Sunshine configured for " << width << "x"
                      << height << " → 960x544 downscale (host mode intact)" << std::endl;
            applied = true;

            // Reiniciar Sunshine solo si realmente cambió la configuración
            if (sunshine_webui_->restartSunshine()) {
                std::cout << "[AudioReceiver] Sunshine restart requested after resolution change" << std::endl;
            } else {
                std::cerr << "[AudioReceiver] Sunshine restart request failed" << std::endl;
            }
        } else {
            std::cerr << "[AudioReceiver] Sunshine WebUI failed to apply resolution" << std::endl;
        }
    } else {
        std::cerr << "[AudioReceiver] Sunshine WebUI not available - skipping API resolution change" << std::endl;
    }

    // Do not force host display changes; rely solely on Sunshine remapping.
    return applied;
}

bool AudioReceiver::applyFallbackDisplayResolution(uint16_t width, uint16_t height) {
    // Host resolution should remain untouched. Disable fallback.
    std::cout << "[AudioReceiver] Fallback display resolution disabled (host mode unchanged)" << std::endl;
    return false;
}

AudioReceiver::Stats AudioReceiver::getStats() {
    // Update connection status
    stats_.is_connected = client_validated_;
    stats_.is_paused = paused_;
    
    // Check for timeout if we think we are connected
    if (stats_.is_connected && connection_monitor_ && last_validated_time_.time_since_epoch().count() > 0) {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_packet_time_);
        
        // Timeout logic: 2 seconds without packets = partially disconnected
        // 4 seconds = Assume full disconnect, reset handshake
        if (diff.count() > 2000) {
             stats_.is_receiving = false;
             // We don't mark is_connected = false immediately, we wait for longer timeout
             if (diff.count() > 4000) {
                 stats_.is_connected = false;
                 // Don't reset connection state fully, allow quick resume
                 // If we were connected, and now we are not, reset the state
                 if (client_validated_) {
                     std::cout << "[AudioReceiver] Client disconnected (timeout): " << client_devicename_ << std::endl;
                     resetConnectionState();
                 }
             }
        }
    }
    return stats_;
}

} // namespace moonmic

