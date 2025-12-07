/**
 * @file sunshine_webui.cpp
 * @brief Sunshine Web UI API client implementation
 */

#include "sunshine_webui.h"
#include "config.h"
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

// For Base64 encoding
#include <sstream>
#include <iomanip>

extern bool g_debug_mode;  // From main.cpp

namespace moonmic {

// Simple Base64 encode (for HTTP Basic Auth)
static std::string base64_encode(const std::string& input) {
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string output;
    int val = 0;
    int valb = -6;
    
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            output.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        output.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (output.size() % 4) {
        output.push_back('=');
    }
    
    return output;
}

// Simple XOR encryption/decryption for credentials
static std::string xor_encrypt(const std::string& data, const std::string& key) {
    std::string result = data;
    for (size_t i = 0; i < data.size(); ++i) {
        result[i] = data[i] ^ key[i % key.size()];
    }
    return result;
}

SunshineWebUI::SunshineWebUI(Config& config) 
    : config_(config) {
    loadCredentials();
    
    if (isLoggedIn()) {
        std::cout << "[SunshineWebUI] Found saved credentials, validating session..." << std::endl;
        // Validate credentials by attempting to refresh client list
        if (refreshClientList()) {
            config_.sunshine.paired = true;
            std::cout << "[SunshineWebUI] Session validated - auto-login successful" << std::endl;
        } else {
            // Credentials are invalid/expired
            config_.sunshine.paired = false;
            std::cout << "[SunshineWebUI] Session expired - please login again" << std::endl;
        }
    }
}

std::string SunshineWebUI::generateAuthHeader() const {
    if (config_.sunshine.webui_username.empty()) {
        return "";
    }
    
    // Decrypt password
    std::string password = xor_encrypt(
        config_.sunshine.webui_password_encrypted,
        "moonmic_sunshine_key"
    );
    
    // Create "username:password" string
    std::string credentials = config_.sunshine.webui_username + ":" + password;
    
    // Base64 encode
    std::string encoded = base64_encode(credentials);
    
    return "Basic " + encoded;
}

std::string SunshineWebUI::makeAuthenticatedRequest(
    const std::string& endpoint,
    const std::string& method,
    const std::string& body
) {
    if (!isLoggedIn()) {
        std::cerr << "[SunshineWebUI] Not logged in" << std::endl;
        return "";
    }
    
    // Build URL
    std::string url = "https://" + config_.sunshine.host + ":" + 
                      std::to_string(config_.sunshine.webui_port) + endpoint;
    
    // Generate auth header
    std::string auth_header = generateAuthHeader();
    if (auth_header.empty()) {
        std::cerr << "[SunshineWebUI] Failed to generate auth header" << std::endl;
        return "";
    }
    
    if (g_debug_mode) {
        std::cout << "[SunshineWebUI] Request: " << method << " " << url << std::endl;
    }
    
    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[SunshineWebUI] Failed to initialize curl" << std::endl;
        return "";
    }
    
    std::string response_string;
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    // Set HTTP method
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }
    }
    
    // Set custom headers
    struct curl_slist* headers = NULL;
    std::string auth_header_full = "Authorization: " + auth_header;
    headers = curl_slist_append(headers, auth_header_full.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Disable SSL verification (Sunshine uses self-signed cert)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
        +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
            std::string* str = static_cast<std::string*>(userdata);
            str->append(ptr, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    // Check for errors
    if (res != CURLE_OK) {
        std::cerr << "[SunshineWebUI] curl_easy_perform() failed: " 
                  << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return "";
    }
    
    // Check HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    if (g_debug_mode) {
        std::cout << "[SunshineWebUI] HTTP " << http_code << " - " 
                  << response_string.size() << " bytes received" << std::endl;
    }
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    // Check if authentication failed
    if (http_code == 401) {
        std::cerr << "[SunshineWebUI] Authentication failed (401 Unauthorized)" << std::endl;
        return "";
    }
    
    if (http_code != 200) {
        std::cerr << "[SunshineWebUI] HTTP error: " << http_code << std::endl;
        return "";
    }
    
    return response_string;
}

bool SunshineWebUI::login(const std::string& username, const std::string& password) {
    std::cout << "[SunshineWebUI] Attempting login for user: " << username << std::endl;
    
    // Temporarily set credentials to test login
    std::string old_username = config_.sunshine.webui_username;
    std::string old_password = config_.sunshine.webui_password_encrypted;
    bool old_logged_in = config_.sunshine.webui_logged_in;
    
    config_.sunshine.webui_username = username;
    config_.sunshine.webui_password_encrypted = xor_encrypt(password, "moonmic_sunshine_key");
    config_.sunshine.webui_logged_in = true;
    
    // Test credentials by making a request to /api/clients/list
    std::string response = makeAuthenticatedRequest("/api/clients/list");
    
    if (response.empty()) {
        // Login failed - restore old credentials
        config_.sunshine.webui_username = old_username;
        config_.sunshine.webui_password_encrypted = old_password;
        config_.sunshine.webui_logged_in = old_logged_in;
        
        std::cerr << "[SunshineWebUI] Login failed - invalid credentials" << std::endl;
        return false;
    }
    
    // Login successful - mark as paired and save credentials
    config_.sunshine.paired = true;
    saveCredentials();
    
    // Parse and load client list from response
    try {
        nlohmann::json json_response = nlohmann::json::parse(response);
        
        if (json_response.contains("named_certs")) {
            paired_clients_.clear();
            
            for (const auto& cert : json_response["named_certs"]) {
                WebUIPairedClient client;
                client.name = cert.value("name", "Unknown");
                client.uuid = cert.value("uuid", "");
                paired_clients_.push_back(client);
            }
            
            std::cout << "[SunshineWebUI] Login successful - loaded " << paired_clients_.size() 
                      << " paired client(s)" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[SunshineWebUI] Error parsing client list: " << e.what() << std::endl;
    }
    
    return true;
}

bool SunshineWebUI::isLoggedIn() const {
    return config_.sunshine.webui_logged_in && 
           !config_.sunshine.webui_username.empty();
}

void SunshineWebUI::logout() {
    config_.sunshine.webui_username = "";
    config_.sunshine.webui_password_encrypted = "";
    config_.sunshine.webui_logged_in = false;
    config_.sunshine.paired = false;
    paired_clients_.clear();
    
    saveCredentials();
    
    std::cout << "[SunshineWebUI] Logged out" << std::endl;
}

std::vector<WebUIPairedClient> SunshineWebUI::getPairedClients() {
    if (!isLoggedIn()) {
        return {};
    }
    
    return paired_clients_;
}

// NOTE: isClientAuthorized() removed - UUID verification not possible
// because Sunshine generates random UUID during pairing (nvhttp.cpp line 274),
// ignoring client's uniqueid. Whitelist validation uses pair_status instead.

bool SunshineWebUI::refreshClientList() {
    if (!isLoggedIn()) {
        return false;
    }
    
    std::cout << "[SunshineWebUI] Refreshing client list..." << std::endl;
    
    std::string response = makeAuthenticatedRequest("/api/clients/list");
    
    if (response.empty()) {
        std::cerr << "[SunshineWebUI] Failed to refresh client list" << std::endl;
        return false;
    }
    
    try {
        nlohmann::json json_response = nlohmann::json::parse(response);
        
        if (json_response.contains("named_certs")) {
            paired_clients_.clear();
            
            for (const auto& cert : json_response["named_certs"]) {
                WebUIPairedClient client;
                client.name = cert.value("name", "Unknown");
                client.uuid = cert.value("uuid", "");
                paired_clients_.push_back(client);
            }
            
            std::cout << "[SunshineWebUI] Loaded " << paired_clients_.size() 
                      << " paired client(s)" << std::endl;
        }
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[SunshineWebUI] Error parsing client list: " << e.what() << std::endl;
        return false;
    }
}

void SunshineWebUI::saveCredentials() {
    // Save to config file
    config_.save(Config::getDefaultConfigPath());
    
    std::cout << "[SunshineWebUI] Credentials saved" << std::endl;
}

void SunshineWebUI::loadCredentials() {
    // Credentials are loaded from config automatically
    if (isLoggedIn()) {
        std::cout << "[SunshineWebUI] Loaded credentials for: " 
                  << config_.sunshine.webui_username << std::endl;
    }
}

} // namespace moonmic
