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
    // Windows: First try to find Sunshine executable in Program Files
    std::vector<std::string> search_paths = {
        "C:\\Program Files\\Sunshine",
        "C:\\Program Files (x86)\\Sunshine"
    };
    
    // Check if sunshine.exe exists in these paths
    for (const auto& path : search_paths) {
        std::string exe_path = path + "\\sunshine.exe";
        std::ifstream exe_check(exe_path);
        if (exe_check.good()) {
            std::cout << "[Sunshine] Found executable at: " << exe_path << std::endl;
            return path;
        }
    }
    
    // Fallback: Check %APPDATA%\Sunshine
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        return std::string(appdata) + "\\Sunshine";
    }
#else
    // Linux: Check common installation paths
    std::vector<std::string> search_paths = {
        "/usr/local/bin",
        "/usr/bin",
        "/opt/sunshine"
    };
    
    for (const auto& path : search_paths) {
        std::string exe_path = path + "/sunshine";
        std::ifstream exe_check(exe_path);
        if (exe_check.good()) {
            std::cout << "[Sunshine] Found executable at: " << exe_path << std::endl;
            return path;
        }
    }
    
    // Fallback: Check ~/.config/sunshine
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
    
    // Try multiple possible locations for the state file
    std::vector<std::string> possible_paths;
    
#ifdef _WIN32
    // Windows: Try config/sunshine_state.json first (relative to executable)
    possible_paths.push_back(sunshine_dir + "\\config\\sunshine_state.json");
    
    // Also check in %APPDATA%\Sunshine\config\sunshine_state.json
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        possible_paths.push_back(std::string(appdata) + "\\Sunshine\\config\\sunshine_state.json");
    }
    
    // Legacy locations (older Sunshine versions)
    possible_paths.push_back(sunshine_dir + "\\sunshine_state.json");
    possible_paths.push_back(sunshine_dir + "\\state.json");
#else
    // Linux: Try config/sunshine_state.json
    possible_paths.push_back(sunshine_dir + "/config/sunshine_state.json");
    
    // Also check ~/.config/sunshine
    const char* home = getenv("HOME");
    if (home) {
        possible_paths.push_back(std::string(home) + "/.config/sunshine/sunshine_state.json");
        possible_paths.push_back(std::string(home) + "/.config/sunshine/config/sunshine_state.json");
    }
    
    // Legacy locations
    possible_paths.push_back(sunshine_dir + "/sunshine_state.json");
    possible_paths.push_back(sunshine_dir + "/state.json");
#endif
    
    // Check each possible path
    for (const auto& path : possible_paths) {
        std::ifstream f(path);
        if (f.good()) {
            std::cout << "[Sunshine] Found state file at: " << path << std::endl;
            return path;
        }
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
        
        // Sunshine stores paired clients in root.named_devices array
        nlohmann::json devices;
        
        // Try root.named_devices first (newer format)
        if (state.contains("root") && state["root"].contains("named_devices") && 
            state["root"]["named_devices"].is_array()) {
            devices = state["root"]["named_devices"];
        }
        // Fallback to top-level named_devices (older format)
        else if (state.contains("named_devices") && state["named_devices"].is_array()) {
            devices = state["named_devices"];
        }
        else {
            std::cout << "[Sunshine] No named_devices found in state file" << std::endl;
            return false;
        }
        
        for (const auto& device : devices) {
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
