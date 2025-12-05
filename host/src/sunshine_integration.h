/**
 * @file sunshine_integration.h
 * @brief Simplified Sunshine integration - only tracks pairing status from config
 */

#pragma once

#include <string>

namespace moonmic {

// Forward declaration
class Config;

/**
 * @brief Sunshine integration manager (simplified)
 * Only tracks pairing status from config, no file parsing
 */
class SunshineIntegration {
public:
    SunshineIntegration(Config& config);
    
    /**
     * @brief Check if moonmic-host is paired with Sunshine
     * @return true if config says we're paired
     */
    bool isPaired() const;
    
    /**
     * @brief Reload paired status from config
     */
    void reload();
    
private:
    Config& config_;
};

} // namespace moonmic
