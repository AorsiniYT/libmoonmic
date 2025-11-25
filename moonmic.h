/**
 * @file moonmic.h
 * @brief Public API for MoonMic - Microphone capture and transmission for Moonlight clients
 * 
 * This library provides cross-platform microphone capture and transmission
 * from Moonlight clients to the host via UDP.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to a MoonMic client instance
 */
typedef struct moonmic_client_t moonmic_client_t;

/**
 * @brief Configuration for MoonMic client
 */
typedef struct {
    /** IP address of the host (required) */
    const char* host_ip;
    
    /** UDP port for transmission (default: 48100) */
    uint16_t port;
    
    /** Audio sample rate in Hz (default: 48000) */
    uint32_t sample_rate;
    
    /** Number of audio channels: 1 (mono) or 2 (stereo) (default: 1) */
    uint8_t channels;
    
    /** Opus bitrate in bits/second (default: 64000) */
    uint32_t bitrate;
    
    /** Auto-start transmission when created (default: true) */
    bool auto_start;
} moonmic_config_t;

/**
 * @brief Error callback function type
 * @param error Error message (null-terminated string)
 * @param userdata User-provided data pointer
 */
typedef void (*moonmic_error_callback_t)(const char* error, void* userdata);

/**
 * @brief Status callback function type
 * @param connected True if connected and transmitting, false otherwise
 * @param userdata User-provided data pointer
 */
typedef void (*moonmic_status_callback_t)(bool connected, void* userdata);

/**
 * @brief Create a new MoonMic client instance
 * @param config Configuration parameters (must not be NULL)
 * @return Pointer to client instance, or NULL on failure
 */
moonmic_client_t* moonmic_create(const moonmic_config_t* config);

/**
 * @brief Destroy a MoonMic client instance
 * @param client Client instance to destroy (can be NULL)
 */
void moonmic_destroy(moonmic_client_t* client);

/**
 * @brief Start microphone capture and transmission
 * @param client Client instance
 * @return true on success, false on failure
 */
bool moonmic_start(moonmic_client_t* client);

/**
 * @brief Stop microphone capture and transmission
 * @param client Client instance
 */
void moonmic_stop(moonmic_client_t* client);

/**
 * @brief Check if the client is currently active (capturing and transmitting)
 * @param client Client instance
 * @return true if active, false otherwise
 */
bool moonmic_is_active(moonmic_client_t* client);

/**
 * @brief Set error callback
 * @param client Client instance
 * @param callback Callback function (can be NULL to disable)
 * @param userdata User data to pass to callback
 */
void moonmic_set_error_callback(moonmic_client_t* client, 
                                moonmic_error_callback_t callback, 
                                void* userdata);

/**
 * @brief Set status callback
 * @param client Client instance
 * @param callback Callback function (can be NULL to disable)
 * @param userdata User data to pass to callback
 */
void moonmic_set_status_callback(moonmic_client_t* client,
                                 moonmic_status_callback_t callback,
                                 void* userdata);

/**
 * @brief Get library version string
 * @return Version string (e.g., "1.0.0")
 */
const char* moonmic_version(void);

#ifdef __cplusplus
}
#endif
