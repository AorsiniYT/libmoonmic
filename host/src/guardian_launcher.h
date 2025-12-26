/**
 * @file guardian_launcher.h
 * @brief Guardian process launcher for moonmic-host
 */

#pragma once

#include <string>

namespace moonmic {

/**
 * @brief Guardian process launcher and manager
 * 
 * The Guardian is a separate watchdog process (moonmic-guardian.exe) that monitors the host.
 * If the host process terminates unexpectedly (crash), the Guardian detects the exit code
 * and automatically restores the user's original default microphone.
 * 
 * Communication uses named Windows Events (Local\ namespace):
 * - Shutdown Event: Host signals normal exit. Guardian ignores exit.
 * - Restart Event: Host signals restart request. Guardian restarts Host.
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
     * @brief Signal guardian to restart the host
     */
    static void signalRestart();
    
    /**
     * @brief Check if guardian is running
     * @return true if guardian process is active
     */
    static bool isGuardianRunning();
    
    // Internals exposed for Guardian executable
    // Defined inline to avoid linking issues with Guardian (which doesn't link guardian_launcher.cpp)
    static constexpr const char* SHUTDOWN_EVENT_NAME = "Local\\MoonMicHostShutdown";
    static constexpr const char* RESTART_EVENT_NAME = "Local\\MoonMicHostRestart";
};

} // namespace moonmic
