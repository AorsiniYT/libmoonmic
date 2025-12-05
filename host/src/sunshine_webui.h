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
    
    /**
     * @brief Check if a specific client UUID is paired in Sunshine
     * @param uuid Client UUID to check
     * @return true if client is paired
     */
    bool isClientAuthorized(const std::string& uuid);
    
    /**
     * @brief Reload client list from Sunshine
     * Call this periodically to refresh the whitelist
     */
    bool refreshClientList();
    
private:
    Config& config_;
    std::vector<WebUIPairedClient> paired_clients_;
    
    /**
     * @brief Make authenticated HTTP request to Web UI API
     * @param endpoint API endpoint (e.g., "/api/clients/list")
     * @param method HTTP method (GET or POST)
     * @param body Request body (for POST)
     * @return Response body, empty if failed
     */
    std::string makeAuthenticatedRequest(
        const std::string& endpoint,
        const std::string& method = "GET",
        const std::string& body = ""
    );
    
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
