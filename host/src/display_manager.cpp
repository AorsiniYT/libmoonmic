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
    // OS resolution changes disabled; avoid touching host mode.
}

DisplayManager::~DisplayManager() {
    // No-op: host resolution untouched
}

DisplayManager::Resolution DisplayManager::getCurrentResolution() {
    // Return a neutral resolution without querying OS to avoid side effects
    Resolution res{};
    res.width = 0;
    res.height = 0;
    res.refresh_rate = 0;
    return res;
}

bool DisplayManager::setResolution(int width, int height, int refresh_rate) {
    std::cout << "[DisplayManager] setResolution skipped (OS resolution changes disabled)" << std::endl;
    return false;
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
