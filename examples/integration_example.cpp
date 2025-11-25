/**
 * @file integration_example.cpp
 * @brief Example of how to integrate libmoonmic with GameStreamClient
 * 
 * This file shows how to add microphone support to vita-moonlight
 */

#include "GameStreamClient.hpp"
#include "moonmic.h"
#include <borealis.hpp>

class GameStreamClientWithMic : public GameStreamClient {
private:
    moonmic_client_t* m_moonmic = nullptr;
    bool m_mic_enabled = false;
    
    // Error callback for MoonMic
    static void on_moonmic_error(const char* error, void* userdata) {
        brls::Logger::error("[MoonMic] Error: {}", error);
    }
    
    // Status callback for MoonMic
    static void on_moonmic_status(bool connected, void* userdata) {
        if (connected) {
            brls::Logger::info("[MoonMic] Connected and transmitting");
        } else {
            brls::Logger::info("[MoonMic] Disconnected");
        }
    }
    
public:
    /**
     * @brief Start streaming with optional microphone support
     */
    bool startApp(const std::string& address, 
                  STREAM_CONFIGURATION& config, 
                  int appId,
                  bool enable_microphone = false) {
        
        // Start normal streaming
        bool success = GameStreamClient::startApp(address, config, appId);
        if (!success) {
            return false;
        }
        
        // Start microphone if requested
        if (enable_microphone) {
            startMicrophone(address);
        }
        
        return true;
    }
    
    /**
     * @brief Start microphone transmission
     */
    bool startMicrophone(const std::string& host_address) {
        if (m_moonmic) {
            brls::Logger::warning("[MoonMic] Already started");
            return false;
        }
        
        // Configure MoonMic
        moonmic_config_t config = {
            .host_ip = host_address.c_str(),
            .port = 48100,
            .sample_rate = 48000,
            .channels = 1,  // Mono for voice
            .bitrate = 64000,
            .auto_start = true
        };
        
        // Create MoonMic client
        m_moonmic = moonmic_create(&config);
        if (!m_moonmic) {
            brls::Logger::error("[MoonMic] Failed to create client");
            return false;
        }
        
        // Set callbacks
        moonmic_set_error_callback(m_moonmic, on_moonmic_error, this);
        moonmic_set_status_callback(m_moonmic, on_moonmic_status, this);
        
        // Start transmission (if auto_start is false, call moonmic_start manually)
        if (!moonmic_is_active(m_moonmic)) {
            if (!moonmic_start(m_moonmic)) {
                brls::Logger::error("[MoonMic] Failed to start");
                moonmic_destroy(m_moonmic);
                m_moonmic = nullptr;
                return false;
            }
        }
        
        m_mic_enabled = true;
        brls::Logger::info("[MoonMic] Started successfully");
        return true;
    }
    
    /**
     * @brief Stop microphone transmission
     */
    void stopMicrophone() {
        if (!m_moonmic) {
            return;
        }
        
        moonmic_stop(m_moonmic);
        moonmic_destroy(m_moonmic);
        m_moonmic = nullptr;
        m_mic_enabled = false;
        
        brls::Logger::info("[MoonMic] Stopped");
    }
    
    /**
     * @brief Override quitApp to also stop microphone
     */
    bool quitApp(const std::string& address) override {
        stopMicrophone();
        return GameStreamClient::quitApp(address);
    }
    
    /**
     * @brief Check if microphone is active
     */
    bool isMicrophoneActive() const {
        return m_mic_enabled && m_moonmic && moonmic_is_active(m_moonmic);
    }
    
    /**
     * @brief Get MoonMic version
     */
    static const char* getMoonMicVersion() {
        return moonmic_version();
    }
};

// Example usage in session start
void example_start_session_with_mic() {
    GameStreamClientWithMic client;
    
    std::string host_address = "192.168.1.100";
    STREAM_CONFIGURATION config;
    // ... configure stream settings ...
    
    int app_id = 123456;
    bool enable_mic = true;  // Enable microphone
    
    if (client.startApp(host_address, config, app_id, enable_mic)) {
        brls::Logger::info("Session started with microphone support");
        brls::Logger::info("MoonMic version: {}", client.getMoonMicVersion());
    }
}

// Example: Toggle microphone during session
void example_toggle_microphone(GameStreamClientWithMic& client, const std::string& host) {
    if (client.isMicrophoneActive()) {
        client.stopMicrophone();
        brls::Logger::info("Microphone muted");
    } else {
        client.startMicrophone(host);
        brls::Logger::info("Microphone unmuted");
    }
}
