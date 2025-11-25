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
        int sample_rate = 48000;
        int channels = 1;
        int buffer_size_ms = 20;
    } audio;
    
    // Security settings
    struct {
        bool enable_whitelist = true;
        bool sync_with_sunshine = true;
        std::string sunshine_state_file;
        std::vector<std::string> allowed_clients;
    } security;
    
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
