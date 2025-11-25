/**
 * @file driver_installer.h
 * @brief VB-CABLE driver installer for Windows
 */

#pragma once

#include <string>

namespace moonmic {

class DriverInstaller {
public:
    DriverInstaller();
    
    /**
     * @brief Check if VB-CABLE driver is installed
     * @return true if installed
     */
    bool isVBCableInstalled();
    
    /**
     * @brief Get VB-CABLE Input device name (for writing audio)
     * @return Device name or empty if not found
     */
    std::string getVBCableInputDevice();
    
    /**
     * @brief Get VB-CABLE Output device name (virtual mic)
     * @return Device name or empty if not found
     */
    std::string getVBCableOutputDevice();
    
    /**
     * @brief Install VB-CABLE driver (requires admin)
     * @return true if installation successful
     */
    bool installDriver();
    
    /**
     * @brief Check if running with administrator privileges
     * @return true if admin
     */
    static bool isRunningAsAdmin();
    
    /**
     * @brief Restart application with admin privileges
     * @return true if restart initiated
     */
    static bool restartAsAdmin();
    
private:
    std::string driver_path_;
    
    bool runSetupExecutable();
    bool enumerateAudioDevices();
    bool extractEmbeddedInstaller(const std::string& temp_dir);
    bool extractResourceToFile(const char* resource_name, const std::string& output_path);
    void cleanupTempDir(const std::string& temp_dir);
};

} // namespace moonmic
