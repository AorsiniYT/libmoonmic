/**
 * @file display_settings_gui.h
 * @brief ImGui modal for display settings
 */

#pragma once

#ifdef USE_IMGUI

#include "display_manager.h"

namespace moonmic {

/**
 * @brief GUI for configuring display settings
 */
class DisplaySettingsGUI {
public:
    DisplaySettingsGUI();
    
    /**
     * @brief Render the display settings modal window
     * @param display_mgr Reference to DisplayManager
     */
    void render(DisplayManager& display_mgr);
    
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
    int selected_width_;
    int selected_height_;
    int selected_refresh_;
    bool auto_change_enabled_;
};

} // namespace moonmic

#endif // USE_IMGUI
