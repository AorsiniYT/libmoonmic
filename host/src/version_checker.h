/**
 * @file version_checker.h
 * @brief Version update checker for moonmic-host
 */

#pragma once

#include <string>
#include <functional>

namespace moonmic {

class VersionChecker {
public:
    struct VersionInfo {
        std::string current_version;
        std::string latest_version;
        bool update_available;
        std::string download_url;
    };
    
    VersionChecker();
    ~VersionChecker();
    
    /**
     * @brief Check for updates asynchronously
     * @param callback Called when check completes with version info
     */
    void checkForUpdates(std::function<void(const VersionInfo&)> callback);
    
    /**
     * @brief Get current version from embedded VERSION file
     */
    static std::string getCurrentVersion();
    
    /**
     * @brief Compare two version strings (semantic versioning)
     * @return -1 if v1 < v2, 0 if equal, 1 if v1 > v2
     */
    static int compareVersions(const std::string& v1, const std::string& v2);
    
private:
    std::string fetchLatestVersion();
};

} // namespace moonmic
