/**
 * @file heartbeat_monitor.cpp
 * @brief Linux heartbeat monitor implementation
 */

#include "../heartbeat_monitor.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <cstring>
#include <cstdlib>

#define PING_MAGIC 0x50494E47  // "PING"
#define PING_TIMEOUT_MS 3000   // 3 seconds
#define CTRL_STOP_MAGIC 0x53544F50  // "STOP"
#define CTRL_START_MAGIC 0x53545254  // "STRT"

#pragma pack(push, 1)
struct ping_packet {
    uint32_t magic;
    uint64_t timestamp;
};
#pragma pack(pop)

struct heartbeat_monitor_t {
    int socket;
    volatile int running;
    volatile moonmic_connection_status_t status;
    volatile uint64_t last_ping_time;
    pthread_t thread;
    volatile int paused;  // 1 if host sent STOP, 0 if host sent START
};

// Get time in milliseconds
static uint64_t get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Monitor thread function
static void* monitor_thread_func(void* param) {
    heartbeat_monitor_t* monitor = (heartbeat_monitor_t*)param;
    uint8_t buffer[32];
    
    while (__sync_fetch_and_add(&monitor->running, 0)) {
        // Receive with timeout
        ssize_t received = recv(monitor->socket, buffer, sizeof(buffer), 0);
        
        if (received >= 4) {  // At least magic number
            uint32_t magic;
            memcpy(&magic, buffer, sizeof(magic));
            
            if (magic == PING_MAGIC && received == sizeof(ping_packet)) {
                // Valid PING received
                monitor->last_ping_time = get_time_ms();
                monitor->status = MOONMIC_CONNECTED;
            }
            else if (magic == CTRL_STOP_MAGIC && received == 8) {
                // STOP signal from host - pause transmission
                __sync_lock_test_and_set(&monitor->paused, 1);
            }
            else if (magic == CTRL_START_MAGIC && received == 8) {
                // START signal from host - resume transmission
                __sync_lock_test_and_set(&monitor->paused, 0);
            }
        }
        
        // Check for timeout
        uint64_t now = get_time_ms();
        if (now - monitor->last_ping_time > PING_TIMEOUT_MS) {
            monitor->status = MOONMIC_DISCONNECTED;
        }
    }
    
    return nullptr;
}

extern "C" {

heartbeat_monitor_t* heartbeat_monitor_create(uint16_t port) {
    heartbeat_monitor_t* monitor = (heartbeat_monitor_t*)calloc(1, sizeof(heartbeat_monitor_t));
    if (!monitor) {
        return nullptr;
    }
    
    // Create UDP socket
    monitor->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (monitor->socket < 0) {
        free(monitor);
        return nullptr;
    }
    
    // Set socket timeout (100ms)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms
    setsockopt(monitor->socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(monitor->socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(monitor->socket);
        free(monitor);
        return nullptr;
    }
    
    // Initialize state
    monitor->status = MOONMIC_DISCONNECTED;
    monitor->last_ping_time = 0;
    monitor->running = 1;
    monitor->paused = 0;  // Start unpaused
    
    // Create monitor thread
    if (pthread_create(&monitor->thread, nullptr, monitor_thread_func, monitor) != 0) {
        close(monitor->socket);
        free(monitor);
        return nullptr;
    }
    
    return monitor;
}

void heartbeat_monitor_destroy(heartbeat_monitor_t* monitor) {
    if (!monitor) {
        return;
    }
    
    __sync_lock_test_and_set(&monitor->running, 0);
    
    // Wait for thread to exit
    pthread_join(monitor->thread, nullptr);
    
    close(monitor->socket);
    free(monitor);
}

moonmic_connection_status_t heartbeat_monitor_get_status(heartbeat_monitor_t* monitor) {
    if (!monitor) {
        return MOONMIC_DISCONNECTED;
    }
    return monitor->status;
}

bool heartbeat_monitor_is_connected(heartbeat_monitor_t* monitor) {
    return heartbeat_monitor_get_status(monitor) == MOONMIC_CONNECTED;
}

bool heartbeat_monitor_is_paused(heartbeat_monitor_t* monitor) {
    if (!monitor) {
        return false;
    }
    return __sync_fetch_and_add(&monitor->paused, 0) != 0;
}

} // extern "C"
