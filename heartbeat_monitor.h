/**
 * @file heartbeat_monitor.h
 * @brief Client-side heartbeat monitor for connection detection
 * 
 * Platform-specific implementations:
 * - platform/psvita/heartbeat_monitor.cpp
 * - platform/windows/heartbeat_monitor.cpp  
 * - platform/linux/heartbeat_monitor.cpp
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Connection status enum
 */
typedef enum {
    MOONMIC_DISCONNECTED = 0,
    MOONMIC_CONNECTED = 1
} moonmic_connection_status_t;

/**
 * @brief Heartbeat monitor structure (opaque)
 */
typedef struct heartbeat_monitor_t heartbeat_monitor_t;

/**
 * @brief Create and start heartbeat monitor
 * @param socket_fd Existing socket file descriptor to listen on
 * @param host_ip Host IP string
 * @param host_port Host port associated with the socket
 * @return Monitor instance or NULL on error
 */
heartbeat_monitor_t* heartbeat_monitor_create(int socket_fd, const char* host_ip, uint16_t host_port);

/**
 * @brief Get current round-trip time in milliseconds
 * @param monitor Monitor instance
 * @return RTT in ms, or -1 if not available
 */
int heartbeat_monitor_get_rtt(heartbeat_monitor_t* monitor);

/**
 * @brief Destroy heartbeat monitor
 * @param monitor Monitor instance
 */
void heartbeat_monitor_destroy(heartbeat_monitor_t* monitor);

/**
 * @brief Get current connection status
 * @param monitor Monitor instance
 * @return Connection status
 */
moonmic_connection_status_t heartbeat_monitor_get_status(heartbeat_monitor_t* monitor);

/**
 * @brief Check if connected (convenience function)
 * @param monitor Monitor instance
 * @return true if connected, false otherwise
 */
bool heartbeat_monitor_is_connected(heartbeat_monitor_t* monitor);

/**
 * @brief Check if host has paused transmission (STOP signal received)
 * @param monitor Monitor instance
 * @return true if paused, false otherwise
 */
bool heartbeat_monitor_is_paused(heartbeat_monitor_t* monitor);

#ifdef __cplusplus
}
#endif

