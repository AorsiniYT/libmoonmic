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
    
    // Check if Steam Streaming Microphone driver is installed
    bool isSteamMicrophoneInstalled();
    
    // Check if ANY compatible driver is installed
    bool isAnyDriverInstalled();
    
    // Install VB-CABLE driver (extracts embedded installer)
    bool installVBCable();
    
    // Uninstall VB-CABLE driver
    bool uninstallVBCable();
    
    // Install Steam Streaming Microphone driver (requires external files)
    bool installSteamMicrophone();

    // Uninstall Steam Streaming Microphone driver
    bool uninstallSteamMicrophone();
    
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

    // Helper to create a root enumerated device node (like devcon install)
    bool createRootDevice(const std::string& hardwareId, const std::string& infPath);
    
    // Helper to remove any devices matching a hardware ID (cleanup)
    void removeDevicesByHardwareId(const std::string& hardwareId);

    // Helper to disable the "Steam Streaming Microphone" playback endpoint (Speaker)
    // to avoid confusion with the actual Microphone endpoint.
    bool disableSteamStreamingSpeakers();
};

} // namespace moonmic