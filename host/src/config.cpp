/**
 * @file config.cpp
 * @brief Configuration implementation
 */

#include "config.h"
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace moonmic {

bool Config::load(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[Config] File not found: " << path << std::endl;
            return false;
        }
        
        nlohmann::json j;
        file >> j;
        
        // Load server settings
        if (j.contains("server")) {
            auto& s = j["server"];
            if (s.contains("port")) server.port = s["port"];
            if (s.contains("bind_address")) server.bind_address = s["bind_address"];
        }
        
        // Load audio settings
        if (j.contains("audio")) {
            auto& a = j["audio"];
            if (a.contains("virtual_device_name")) audio.virtual_device_name = a["virtual_device_name"];
            if (a.contains("sample_rate")) audio.sample_rate = a["sample_rate"];
            if (a.contains("channels")) audio.channels = a["channels"];
            if (a.contains("buffer_size_ms")) audio.buffer_size_ms = a["buffer_size_ms"];
        }
        
        // Load security settings
        if (j.contains("security")) {
            auto& sec = j["security"];
            if (sec.contains("enable_whitelist")) security.enable_whitelist = sec["enable_whitelist"];
            if (sec.contains("sync_with_sunshine")) security.sync_with_sunshine = sec["sync_with_sunshine"];
            if (sec.contains("sunshine_state_file")) security.sunshine_state_file = sec["sunshine_state_file"];
            if (sec.contains("allowed_clients") && sec["allowed_clients"].is_array()) {
                security.allowed_clients.clear();
                for (const auto& client : sec["allowed_clients"]) {
                    security.allowed_clients.push_back(client);
                }
            }
        }
        
        // Load GUI settings
        if (j.contains("gui")) {
            auto& g = j["gui"];
            if (g.contains("show_on_startup")) gui.show_on_startup = g["show_on_startup"];
            if (g.contains("minimize_to_tray")) gui.minimize_to_tray = g["minimize_to_tray"];
            if (g.contains("theme")) gui.theme = g["theme"];
        }
        
        std::cout << "[Config] Loaded from: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Config] Error loading: " << e.what() << std::endl;
        return false;
    }
}

bool Config::save(const std::string& path) {
    try {
        nlohmann::json j;
        
        j["server"]["port"] = server.port;
        j["server"]["bind_address"] = server.bind_address;
        
        j["audio"]["virtual_device_name"] = audio.virtual_device_name;
        j["audio"]["sample_rate"] = audio.sample_rate;
        j["audio"]["channels"] = audio.channels;
        j["audio"]["buffer_size_ms"] = audio.buffer_size_ms;
        
        j["security"]["enable_whitelist"] = security.enable_whitelist;
        j["security"]["sync_with_sunshine"] = security.sync_with_sunshine;
        j["security"]["sunshine_state_file"] = security.sunshine_state_file;
        j["security"]["allowed_clients"] = security.allowed_clients;
        
        j["gui"]["show_on_startup"] = gui.show_on_startup;
        j["gui"]["minimize_to_tray"] = gui.minimize_to_tray;
        j["gui"]["theme"] = gui.theme;
        
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "[Config] Cannot write to: " << path << std::endl;
            return false;
        }
        
        file << j.dump(2);
        std::cout << "[Config] Saved to: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Config] Error saving: " << e.what() << std::endl;
        return false;
    }
}

std::string Config::getDefaultConfigPath() {
#ifdef _WIN32
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        std::string config_dir = std::string(appdata) + "\\MoonMic";
        CreateDirectoryA(config_dir.c_str(), NULL);
        return config_dir + "\\moonmic-host.json";
    }
    return "moonmic-host.json";
#else
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    
    if (home) {
        std::string config_dir = std::string(home) + "/.config/moonmic";
        mkdir(config_dir.c_str(), 0755);
        return config_dir + "/moonmic-host.json";
    }
    return "moonmic-host.json";
#endif
}

} // namespace moonmic
