/**
 * @file sunshine_webui.h
 * @brief Sunshine Web UI API client for security validation
 * 
 * This module handles authentication with Sunshine's Web UI to:
 * 1. Validate that incoming audio clients are paired in Sunshine
 * 2. Prevent unauthorized access to the microphone
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace moonmic {

// Forward declaration
class Config;

/**
 * @brief Information about a paired client from Sunshine Web UI
 */
struct WebUIPairedClient {
    std::string name;
    std::string uuid;
};

/**
 * @brief Sunshine Web UI API client
 * Handles authentication and client validation for security
 */
class SunshineWebUI {
public:
    SunshineWebUI(Config& config);
    
    /**
     * @brief Login to Sunshine Web UI with credentials
     * @param username Sunshine Web UI username
     * @param password Sunshine Web UI password
     * @return true if login successful
     */
    bool login(const std::string& username, const std::string& password);
    
    /**
     * @brief Check if currently logged in
     * @return true if valid Web UI session exists
     */
    bool isLoggedIn() const;
    
    /**
     * @brief Logout from Web UI (clear saved credentials)
     */
    void logout();
    
    /**
     * @brief Get list of all paired clients from Sunshine
     * Requires valid login
     * @return Vector of paired clients, empty if not logged in
     */
    std::vector<WebUIPairedClient> getPairedClients();
    
    // NOTE: isClientAuthorized() removed - UUID verification not possible
    // because Sunshine generates random UUID during pairing, ignoring client's uniqueid.
    // Whitelist validation uses pair_status from handshake instead.
    
    /**
     * @brief Reload client list from Sunshine
     * Call this periodically to refresh the whitelist
     */
    bool refreshClientList();
    
    /**
     * @brief Configure display resolution remapping in Sunshine
     * Sets up automatic resolution change when streaming
     * @param target_width Target display width (e.g., 1280, 1920)
     * @param target_height Target display height (e.g., 720, 1080)
     * @return true if configuration applied successfully
     * 
     * This configures Sunshine to:
     * 1. Change host monitor to target_width x target_height
     * 2. Downscale stream to 960x544 for Vita
     * 3. Provide high quality rendering before downscale
     */
    bool setDisplayResolution(uint16_t target_width, uint16_t target_height);

    /**
     * @brief Get current configured resolution from Sunshine
     * @param[out] width Current resolution width
     * @param[out] height Current resolution height
     * @return true if successful
     */
    bool getCurrentResolution(uint16_t& width, uint16_t& height);

        /**
         * @brief Request Sunshine restart via WebUI API
         */
        bool restartSunshine();
    
    /**
     * @brief Remove display resolution remapping from Sunshine
     * Restores original resolution behavior
     * @return true if configuration cleared successfully
     */
    bool clearDisplayResolution();
    
    /**
     * @brief Make authenticated HTTP request to Web UI API
     * @param endpoint API endpoint (e.g., "/api/clients/list")
     * @param method HTTP method (GET or POST)
     * @param body Request body (for POST)
     * @return Response body, empty if failed
     * 
     * Made public to allow SunshineLogMonitor to fetch logs
     */
    std::string makeAuthenticatedRequest(
        const std::string& endpoint,
        const std::string& method = "GET",
        const std::string& body = ""
    );
    
private:
    Config& config_;
    std::vector<WebUIPairedClient> paired_clients_;
    
    /**
     * @brief Generate HTTP Basic Auth header
     * @return Authorization header value
     */
    std::string generateAuthHeader() const;
    
    /**
     * @brief Save Web UI credentials (encrypted)
     */
    void saveCredentials();
    
    /**
     * @brief Load Web UI credentials (encrypted)
     */
    void loadCredentials();
};

} // namespace moonmic
