/**
 * @file sunshine_settings_gui.h
 * @brief Sunshine Settings GUI for managing Sunshine integration
 */

#pragma once

#ifdef USE_IMGUI

#include <string>

namespace moonmic {

// Forward declarations
class SunshineIntegration;
class SunshineWebUI;
class Config;

/**
 * @brief GUI for Sunshine settings and management
 */
class SunshineSettingsGUI {
public:
    SunshineSettingsGUI();
    
    /**
     * @brief Render the Sunshine settings window
     * @param sunshine Sunshine integration instance
     * @param webui Sunshine Web UI client
     * @param config Configuration
     */
    void render(SunshineIntegration& sunshine, SunshineWebUI& webui, Config& config);
    
    /**
     * @brief Open the settings window
     */
    void open();
    
    /**
     * @brief Close the settings window
     */
    void close();
    
    /**
     * @brief Check if window is open
     */
    bool isOpen() const { return is_open_; }
    
private:
    bool is_open_;
};

} // namespace moonmic

#endif // USE_IMGUI
