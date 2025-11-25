/**
 * @file single_instance.h
 * @brief Ensure only one instance of the application runs
 */

#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace moonmic {

class SingleInstance {
public:
    SingleInstance(const std::string& app_name);
    ~SingleInstance();
    
    /**
     * @brief Check if another instance is already running
     * @return true if another instance exists
     */
    bool isAnotherInstanceRunning();
    
    /**
     * @brief Bring existing instance window to front
     */
    void bringExistingToFront();
    
private:
#ifdef _WIN32
    HANDLE mutex_;
    std::string window_title_;
#else
    int lock_fd_;
    std::string lock_file_;
#endif
};

} // namespace moonmic
