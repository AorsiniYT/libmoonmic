/**
 * @file config.h
 * @brief Configuration management for moonmic-host
 */

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace moonmic {

struct Config {
    // Server settings
    struct {
        int port = 48100;
        std::string bind_address = "0.0.0.0";
    } server;
    
    // Audio settings
    struct {
        int stream_sample_rate = 16000;  // Sample rate of incoming stream (e.g., 16kHz from Vita)
        int resampling_rate = 0;  // Output sample rate (0 = auto-detect from device, or manual override)
        int sample_rate = 0;  // Deprecated: use resampling_rate instead (0 = auto-detect)
        int channels = 1;
        int buffer_size_ms = 20;
        bool use_speaker_mode = false;  // true = play to speakers, false = send to VB-Cable
        std::string driver_type = "VBCABLE"; // "VBCABLE", "STEAM"
        std::string driver_device_name = "VB-Audio Virtual Cable"; // For Device Manager
        std::string recording_endpoint_name = "CABLE Output"; // For Audio Mic Setting
        
        
        bool auto_set_default_mic = true;  // Automatically set virtual device as default recording device
        bool use_virtual_mic = true;  // true = use virtual mic (Steam/VBCable), false = use original physical mic
        std::string original_mic_id;  // Backup of the original default mic ID for restoration
    } audio;
    
    // Security settings
    struct {
        bool enable_whitelist = true;
        bool sync_with_sunshine = true;
        std::string sunshine_state_file;
        std::vector<std::string> allowed_clients;
    } security;
    
    // Sunshine settings
    struct {
        std::string host = "localhost";
        int port = 47989;  // GameStream API port
        int webui_port = 47990;  // Web UI API port
        bool paired = false;
        
        // Web UI authentication (for security/advanced features)
        bool webui_logged_in = false;
        std::string webui_username;
        std::string webui_password_encrypted;  // XOR encrypted
    } sunshine;
    
    // GUI settings
    struct {
        bool show_on_startup = true;
        bool minimize_to_tray = true;
        std::string theme = "dark";
    } gui;
    
    /**
     * @brief Load configuration from JSON file
     */
    bool load(const std::string& path);
    
    /**
     * @brief Save configuration to JSON file
     */
    bool save(const std::string& path);
    
    /**
     * @brief Get default config path
     */
    static std::string getDefaultConfigPath();
};

} // namespace moonmic
