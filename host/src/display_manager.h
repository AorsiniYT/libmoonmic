/**
 * @file display_manager.h
 * @brief Cross-platform display resolution management
 */

#pragma once

#include <vector>
#include <string>

namespace moonmic {

/**
 * @brief Manages display resolution changes
 */
class DisplayManager {
public:
    struct Resolution {
        int width;
        int height;
        int refresh_rate;
        
        bool operator==(const Resolution& other) const {
            return width == other.width && 
                   height == other.height && 
                   refresh_rate == other.refresh_rate;
        }
    };
    
    DisplayManager();
    ~DisplayManager();
    
    /**
     * @brief Get current display resolution
     * @return Current resolution
     */
    Resolution getCurrentResolution();
    
    /**
     * @brief Change display resolution
     * @param width Width in pixels
     * @param height Height in pixels
     * @param refresh_rate Refresh rate in Hz (0 = use current)
     * @return true if successful
     */
        bool setResolution(int width, int height, int refresh_rate);
    
    /**
     * @brief Restore original resolution
     * @return true if successful
     */
        bool restoreOriginalResolution();
    
    /**
     * @brief Get list of supported resolutions
     * @return Vector of supported resolutions
     */
    std::vector<Resolution> getSupportedResolutions();
    
    /**
     * @brief Check if resolution was changed
     * @return true if currently using a non-original resolution
     */
    bool isResolutionChanged() const { return resolution_changed_; }
    
private:
    Resolution original_resolution_;
    bool resolution_changed_;
    
#ifdef _WIN32
    bool setResolutionWindows(int width, int height, int refresh_rate);
    std::vector<Resolution> getSupportedResolutionsWindows();
#else
    bool setResolutionLinux(int width, int height, int refresh_rate);
    std::vector<Resolution> getSupportedResolutionsLinux();
#endif
};

} // namespace moonmic
