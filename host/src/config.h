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
        std::string virtual_device_name = "MoonMic Virtual Microphone";
        int stream_sample_rate = 16000;  // Sample rate of incoming stream (e.g., 16kHz from Vita)
        int resampling_rate = 48000;  // Output sample rate (resample from stream to this)
        int sample_rate = 48000;  // Deprecated: use resampling_rate instead
        int channels = 1;
        int buffer_size_ms = 20;
        bool use_speaker_mode = false;  // true = play to speakers, false = send to VB-Cable
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
