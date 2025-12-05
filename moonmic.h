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
#include "heartbeat_monitor.h"
#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MICROPHONE CONFIGURATION CONSTANTS (Vita Hardware)
// ============================================================================
// These are the optimal values for PS Vita microphone hardware.
// Use these constants everywhere to avoid hardcoding values multiple times.

/** Default microphone sample rate (Hz) - Vita hardware native rate */
#define MOONMIC_DEFAULT_SAMPLE_RATE 16000

/** Default microphone channels - Vita only supports mono */
#define MOONMIC_DEFAULT_CHANNELS 1

/** Default Opus bitrate (bps) - optimal for 16kHz mono VOIP */
#define MOONMIC_DEFAULT_BITRATE 64000  // 64kbps for high-quality voice

/** Default UDP port for microphone transmission */
#define MOONMIC_DEFAULT_PORT 48100

// ============================================================================

/**
 * @brief Opaque handle to a MoonMic client instance
 */
typedef struct moonmic_client_t moonmic_client_t;

/**
 * @brief Configuration for MoonMic client
 */
typedef struct {
    const char* host_ip;       /**< Host IP address */
    uint16_t port;            /**< UDP port (default: 48100) */
    uint32_t sample_rate;     /**< Sample rate in Hz (default: 16000) */
    uint8_t channels;         /**< Number of channels (default: 1 = mono) */
    uint32_t bitrate;         /**< Opus bitrate in bps (default: 24000) */
    bool raw_mode;            /**< True = RAW PCM, False = Opus compression */
    bool auto_start;          /**< Auto-start capture after init */
    float gain;               /**< Gain multiplier (1.0-100.0, default: 10.0) */
    
    // NEW: Sunshine validation (optional, can be NULL)
    const char* uniqueid;     /**< Client uniqueid (16 chars, optional) */
    const char* devicename;   /**< Device name for identification (optional) */
    int sunshine_https_port;  /**< Sunshine HTTPS port (default: 47984, 0=skip validation) */
    const char* cert_path;    /**< Path to client certificate PEM (optional) */
    const char* key_path;     /**< Path to client key PEM (optional) */
    int pair_status;          /**< Pair status from Sunshine validation (0=unknown/fail, 1=paired) */
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
 * @brief Update gain multiplier during transmission
 * @param client Client instance
 * @param gain New gain multiplier (1.0 = no change, higher = louder)
 */
void moonmic_set_gain(moonmic_client_t* client, float gain);

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
 * @brief Get current connection status
 * @param client Client instance
 * @return Connection status (MOONMIC_CONNECTED or MOONMIC_DISCONNECTED)
 */
moonmic_connection_status_t moonmic_get_connection_status(moonmic_client_t* client);

/**
 * @brief Check if client is connected to host
 * @param client Client instance
 * @return true if connected, false otherwise
 */
bool moonmic_is_connected(moonmic_client_t* client);

/**
 * @brief Get library version string
 * @return Version string (e.g., "1.0.0")
 */
const char* moonmic_get_version();

#ifdef __cplusplus
}
#endif
