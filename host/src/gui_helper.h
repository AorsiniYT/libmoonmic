/**
 * @file gui_helper.h
 * @brief Helper functions for ImGui tooltips and UI assistance
 */

#pragma once

#include <string>

namespace moonmic {

/**
 * @brief Show a tooltip when hovering over the previous ImGui item
 * @param text Tooltip text to display
 */
void ShowHelpTooltip(const char* text);

/**
 * @brief Show a tooltip with multiple lines
 * @param title Tooltip title (bold)
 * @param description Detailed description
 */
void ShowDetailedTooltip(const char* title, const char* description);

// Specific tooltips for common UI elements
namespace Tooltips {
    // Audio Settings
    extern const char* VIRTUAL_MIC;
    extern const char* ORIGINAL_MIC_SELECTOR;
    extern const char* CURRENT_DEFAULT_MIC;
    
    // Driver Management
    extern const char* VBCABLE_DRIVER;
    extern const char* STEAM_DRIVER;
    extern const char* INSTALL_DRIVER;
    extern const char* UNINSTALL_DRIVER;
    
    // Audio Receiver
    extern const char* AUDIO_RECEIVER_STATUS;
    extern const char* SAMPLE_RATE;
    extern const char* BUFFER_SIZE;
    
    // Sunshine Integration
    extern const char* SUNSHINE_AUTO_CONFIG;
    extern const char* SUNSHINE_WEBUI;
    extern const char* RELOAD_SUNSHINE;
    
    // Guardian System
    extern const char* GUARDIAN_STATUS;
    
    // Debug Options
    extern const char* DEBUG_MODE;
    extern const char* SPEAKER_MODE;
    
    // Configuration
    extern const char* WHITELIST;
    extern const char* PORT_CONFIG;
    extern const char* CHANNELS_CONFIG;
    
    // Controls
    extern const char* PAUSE_RESUME;
    extern const char* DISPLAY_SETTINGS;
    extern const char* PACKET_STATS;
}

} // namespace moonmic
