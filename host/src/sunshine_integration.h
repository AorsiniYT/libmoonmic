/**
 * @file sunshine_integration.h
 * @brief Integration with Sunshine to read paired clients
 */

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace moonmic {

/**
 * @brief Information about a paired Sunshine client
 */
struct PairedClient {
    std::string name;
    std::string uuid;
};

/**
 * @brief Sunshine integration manager
 */
class SunshineIntegration {
public:
    SunshineIntegration();
    
    /**
     * @brief Detect if Sunshine is installed
     * @return true if Sunshine is found
     */
    bool detectSunshine();
    
    /**
     * @brief Get the path to Sunshine's state file
     * @return Path to state file, or empty if not found
     */
    std::string getSunshineStatePath();
    
    /**
     * @brief Load paired clients from Sunshine
     * @return List of paired clients
     */
    std::vector<PairedClient> loadPairedClients();
    
    /**
     * @brief Check if a client UUID is in the paired list
     * @param uuid Client UUID to check
     * @return true if client is paired
     */
    bool isClientPaired(const std::string& uuid);
    
    /**
     * @brief Reload paired clients from Sunshine
     */
    void reload();
    
private:
    std::string state_file_path_;
    std::vector<PairedClient> paired_clients_;
    bool sunshine_detected_;
    
    /**
     * @brief Find Sunshine installation directory
     */
    std::string findSunshineDir();
    
    /**
     * @brief Parse Sunshine state file
     */
    bool parseStateFile(const std::string& path);
};

} // namespace moonmic
