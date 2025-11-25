/**
 * @file sunshine_integration.cpp
 * @brief Implementation of Sunshine integration
 */

#include "sunshine_integration.h"
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#endif

namespace moonmic {

SunshineIntegration::SunshineIntegration()
    : sunshine_detected_(false) {
    detectSunshine();
}

bool SunshineIntegration::detectSunshine() {
    state_file_path_ = getSunshineStatePath();
    sunshine_detected_ = !state_file_path_.empty();
    
    if (sunshine_detected_) {
        std::cout << "[Sunshine] Detected at: " << state_file_path_ << std::endl;
        reload();
    } else {
        std::cout << "[Sunshine] Not detected" << std::endl;
    }
    
    return sunshine_detected_;
}

std::string SunshineIntegration::findSunshineDir() {
#ifdef _WIN32
    // Windows: Check %APPDATA%\Sunshine
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        return std::string(appdata) + "\\Sunshine";
    }
#else
    // Linux: Check ~/.config/sunshine
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }
    
    if (home) {
        return std::string(home) + "/.config/sunshine";
    }
#endif
    return "";
}

std::string SunshineIntegration::getSunshineStatePath() {
    std::string sunshine_dir = findSunshineDir();
    if (sunshine_dir.empty()) {
        return "";
    }
    
#ifdef _WIN32
    std::string state_file = sunshine_dir + "\\state.json";
#else
    std::string state_file = sunshine_dir + "/state.json";
#endif
    
    // Check if file exists
    std::ifstream f(state_file);
    if (f.good()) {
        return state_file;
    }
    
    return "";
}

bool SunshineIntegration::parseStateFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        
        nlohmann::json state;
        file >> state;
        
        paired_clients_.clear();
        
        // Sunshine stores paired clients in "named_devices" array
        if (state.contains("named_devices") && state["named_devices"].is_array()) {
            for (const auto& device : state["named_devices"]) {
                PairedClient client;
                
                if (device.contains("name") && device["name"].is_string()) {
                    client.name = device["name"].get<std::string>();
                }
                
                if (device.contains("uuid") && device["uuid"].is_string()) {
                    client.uuid = device["uuid"].get<std::string>();
                }
                
                if (!client.uuid.empty()) {
                    paired_clients_.push_back(client);
                    std::cout << "[Sunshine] Found paired client: " 
                              << client.name << " (" << client.uuid << ")" << std::endl;
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Sunshine] Error parsing state file: " << e.what() << std::endl;
        return false;
    }
}

std::vector<PairedClient> SunshineIntegration::loadPairedClients() {
    if (!state_file_path_.empty()) {
        parseStateFile(state_file_path_);
    }
    return paired_clients_;
}

bool SunshineIntegration::isClientPaired(const std::string& uuid) {
    for (const auto& client : paired_clients_) {
        if (client.uuid == uuid) {
            return true;
        }
    }
    return false;
}

void SunshineIntegration::reload() {
    loadPairedClients();
}

} // namespace moonmic
