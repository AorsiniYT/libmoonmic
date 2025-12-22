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
    
    // Check for VBCable devices
    std::string getVBCableInputDevice();
    std::string getVBCableOutputDevice();
    
    // Check if running with Admin privileges
    static bool isRunningAsAdmin();
    
    // Request restart as Admin
    static bool restartAsAdmin();

    // Check if VB-CABLE driver is installed
    bool isVBCableInstalled();
    
    // Check if Steam Streaming Speakers/Microphone driver is installed
    bool isSteamSpeakersInstalled();
    
    // Check if ANY compatible driver is installed
    bool isAnyDriverInstalled();
    
    // Install VB-CABLE driver (extracts embedded installer)
    bool installVBCable();
    
    // Uninstall VB-CABLE driver
    bool uninstallVBCable();
    
    // Install Steam Streaming Speakers/Microphone driver (requires external files)
    bool installSteamSpeakers();

    // Uninstall Steam Streaming Speakers/Microphone driver
    bool uninstallSteamSpeakers();
    
private:
    std::string driver_path_;
    
    // Helper to extract embedded resource
    bool extractResourceToFile(const char* resource_name, const std::string& output_path);
    
    // Helper to extract Steam drivers
    bool extractEmbeddedSteamDriver(const std::string& temp_dir, bool is_x64);
    
    // Helper to extract and run embedded installer
    bool runSetupExecutable(bool uninstall = false);
    
    // Helper to extract embedded installer files
    bool extractEmbeddedInstaller(const std::string& temp_dir);
    void cleanupTempDir(const std::string& temp_dir);
};

} // namespace moonmic