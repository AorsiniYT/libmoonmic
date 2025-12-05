/**
 * @file display_manager.cpp
 * @brief Implementation of display resolution management
 */

#include "display_manager.h"
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#include <sstream>
#include <fstream>
#endif

namespace moonmic {

DisplayManager::DisplayManager()
    : resolution_changed_(false) {
    // Save current resolution on construction
    original_resolution_ = getCurrentResolution();
    std::cout << "[DisplayManager] Original resolution: " 
              << original_resolution_.width << "x" << original_resolution_.height 
              << "@" << original_resolution_.refresh_rate << "Hz" << std::endl;
}

DisplayManager::~DisplayManager() {
    // Restore original resolution on destruction
    if (resolution_changed_) {
        std::cout << "[DisplayManager] Restoring original resolution on destroy" << std::endl;
        restoreOriginalResolution();
    }
}

DisplayManager::Resolution DisplayManager::getCurrentResolution() {
    Resolution res{};
    
#ifdef _WIN32
    DEVMODE dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    
    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        res.width = dm.dmPelsWidth;
        res.height = dm.dmPelsHeight;
        res.refresh_rate = dm.dmDisplayFrequency;
    }
#else
    // Linux: parse xrandr output
    FILE* pipe = popen("xrandr | grep '*' | awk '{print $1}'", "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            // Parse "1920x1080" format
            if (sscanf(buffer, "%dx%d", &res.width, &res.height) == 2) {
                // Get refresh rate
                FILE* rate_pipe = popen("xrandr | grep '*' | awk '{print $2}' | tr -d '*+'", "r");
                if (rate_pipe) {
                    char rate_buf[32];
                    if (fgets(rate_buf, sizeof(rate_buf), rate_pipe)) {
                        res.refresh_rate = static_cast<int>(atof(rate_buf));
                    }
                    pclose(rate_pipe);
                }
            }
        }
        pclose(pipe);
    }
#endif
    
    return res;
}

bool DisplayManager::setResolution(int width, int height, int refresh_rate) {
    std::cout << "[DisplayManager] Setting resolution: " 
              << width << "x" << height << "@" << refresh_rate << "Hz" << std::endl;
    
    bool success = false;
    
#ifdef _WIN32
    success = setResolutionWindows(width, height, refresh_rate);
#else
    success = setResolutionLinux(width, height, refresh_rate);
#endif
    
    if (success) {
        resolution_changed_ = true;
        std::cout << "[DisplayManager] Resolution changed successfully" << std::endl;
    } else {
        std::cerr << "[DisplayManager] Failed to change resolution" << std::endl;
    }
    
    return success;
}

bool DisplayManager::restoreOriginalResolution() {
    if (!resolution_changed_) {
        return true;
    }
    
    std::cout << "[DisplayManager] Restoring original resolution: " 
              << original_resolution_.width << "x" << original_resolution_.height 
              << "@" << original_resolution_.refresh_rate << "Hz" << std::endl;
    
    bool success = setResolution(
        original_resolution_.width,
        original_resolution_.height,
        original_resolution_.refresh_rate
    );
    
    if (success) {
        resolution_changed_ = false;
    }
    
    return success;
}

std::vector<DisplayManager::Resolution> DisplayManager::getSupportedResolutions() {
#ifdef _WIN32
    return getSupportedResolutionsWindows();
#else
    return getSupportedResolutionsLinux();
#endif
}

#ifdef _WIN32

bool DisplayManager::setResolutionWindows(int width, int height, int refresh_rate) {
    DEVMODE dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    dm.dmPelsWidth = width;
    dm.dmPelsHeight = height;
    dm.dmDisplayFrequency = refresh_rate;
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
    
    LONG result = ChangeDisplaySettings(&dm, CDS_FULLSCREEN);
    
    if (result == DISP_CHANGE_SUCCESSFUL) {
        return true;
    } else {
        std::cerr << "[DisplayManager] ChangeDisplaySettings failed with code: " << result << std::endl;
        return false;
    }
}

std::vector<DisplayManager::Resolution> DisplayManager::getSupportedResolutionsWindows() {
    std::vector<Resolution> resolutions;
    DEVMODE dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    
    for (int i = 0; EnumDisplaySettings(NULL, i, &dm); i++) {
        Resolution res;
        res.width = dm.dmPelsWidth;
        res.height = dm.dmPelsHeight;
        res.refresh_rate = dm.dmDisplayFrequency;
        
        // Avoid duplicates
        if (std::find(resolutions.begin(), resolutions.end(), res) == resolutions.end()) {
            resolutions.push_back(res);
        }
    }
    
    // Sort by resolution (width * height)
    std::sort(resolutions.begin(), resolutions.end(), 
        [](const Resolution& a, const Resolution& b) {
            return (a.width * a.height) < (b.width * b.height);
        }
    );
    
    return resolutions;
}

#else // Linux

bool DisplayManager::setResolutionLinux(int width, int height, int refresh_rate) {
    // Try xrandr first
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
             "xrandr --output $(xrandr | grep ' connected' | awk '{print $1}' | head -1) --mode %dx%d --rate %d 2>/dev/null",
             width, height, refresh_rate);
    
    int result = system(cmd);
    
    if (result == 0) {
        return true;
    }
    
    // Fallback: try kscreen-doctor (KDE Plasma)
    snprintf(cmd, sizeof(cmd),
             "kscreen-doctor output.1.mode.%dx%d@%d 2>/dev/null",
             width, height, refresh_rate);
    
    result = system(cmd);
    return result == 0;
}

std::vector<DisplayManager::Resolution> DisplayManager::getSupportedResolutionsLinux() {
    std::vector<Resolution> resolutions;
    
    FILE* pipe = popen("xrandr | grep -oP '\\d+x\\d+' | sort -u", "r");
    if (!pipe) {
        return resolutions;
    }
    
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        Resolution res;
        if (sscanf(buffer, "%dx%d", &res.width, &res.height) == 2) {
            res.refresh_rate = 60; // Default, could parse from xrandr detail
            resolutions.push_back(res);
        }
    }
    
    pclose(pipe);
    return resolutions;
}

#endif

} // namespace moonmic
