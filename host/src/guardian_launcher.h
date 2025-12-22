/**
 * @file guardian_launcher.h
 * @brief Guardian process launcher for moonmic-host
 */

#pragma once

#include <string>

namespace moonmic {

/**
 * @brief Guardian process launcher and manager
 */
class GuardianLauncher {
public:
    /**
     * @brief Launch the guardian watchdog process
     * @param original_mic_id Device ID to restore on crash
     * @param original_mic_name Friendly name for logging
     * @return true if guardian launched successfully
     */
    static bool launchGuardian(const std::string& original_mic_id, 
                               const std::string& original_mic_name);
    
    /**
     * @brief Signal guardian for normal shutdown
     * Tells guardian that host is closing cleanly, no restoration needed
     */
    static void signalNormalShutdown();
    
    /**
     * @brief Check if guardian is running
     * @return true if guardian process is active
     */
    static bool isGuardianRunning();
    
private:
    static const char* SHUTDOWN_EVENT_NAME;
};

} // namespace moonmic
