/**
 * @file heartbeat_monitor.cpp
 * @brief PS Vita heartbeat monitor implementation
 */

#include "../heartbeat_monitor.h"
#include <psp2/kernel/processmgr.h>
#include <psp2/net/net.h>
#include <psp2/kernel/threadmgr.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>

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
    SceUID thread_id;
    volatile int paused;  // 1 if host sent STOP, 0 if host sent START
};

// Get time in milliseconds
static uint64_t get_time_ms() {
    return sceKernelGetProcessTimeLow() / 1000;
}

static int monitor_thread_func(SceSize args, void* argp) {
    // Safety check for arguments
    if (!argp) {
        printf("[heartbeat_mon] Error: argp is NULL\n");
        return sceKernelExitDeleteThread(1);
    }

    // Dereference the pointer-to-pointer passed by sceKernelStartThread
    heartbeat_monitor_t* monitor = *(heartbeat_monitor_t**)argp;
    
    if (!monitor) {
        printf("[heartbeat_mon] Error: monitor is NULL\n");
        return sceKernelExitDeleteThread(1);
    }

    printf("[heartbeat_mon] Thread started. Socket: %d\n", monitor->socket);

    // Buffer for receiving data (aligned)
    uint8_t buffer[32]; 
    
    while (monitor->running) {
        // Receive with timeout
        int received = sceNetRecv(monitor->socket, buffer, sizeof(buffer), 0);
        
        if (received >= 4) {  // At least magic number
            // Use memcpy to safely read potentially unaligned data
            uint32_t magic;
            memcpy(&magic, buffer, sizeof(magic));
            
            if (magic == PING_MAGIC && received == sizeof(ping_packet)) {
                // Valid PING received
                monitor->last_ping_time = get_time_ms();
                monitor->status = MOONMIC_CONNECTED;
                // printf("[heartbeat_mon] PING received\n"); // Uncomment for verbose debug
            }
            else if (magic == CTRL_STOP_MAGIC && received == 8) {
                // STOP signal from host - pause transmission
                monitor->paused = 1;
                printf("[heartbeat_mon] Received STOP signal - client paused\n");
            }
            else if (magic == CTRL_START_MAGIC && received == 8) {
                // START signal from host - resume transmission
                monitor->paused = 0;
                printf("[heartbeat_mon] Received START signal - client resumed\n");
            }
        }
        
        // Check for timeout
        uint64_t now = get_time_ms();
        if (now - monitor->last_ping_time > PING_TIMEOUT_MS) {
            if (monitor->status == MOONMIC_CONNECTED) {
                printf("[heartbeat_mon] Connection timed out\n");
            }
            monitor->status = MOONMIC_DISCONNECTED;
        }
    }
    
    printf("[heartbeat_mon] Thread exiting\n");
    return sceKernelExitDeleteThread(0);
}

extern "C" {

heartbeat_monitor_t* heartbeat_monitor_create(uint16_t port) {
    heartbeat_monitor_t* monitor = (heartbeat_monitor_t*)calloc(1, sizeof(heartbeat_monitor_t));
    if (!monitor) {
        return nullptr;
    }
    
    // Create UDP socket
    monitor->socket = sceNetSocket("heartbeat", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
    if (monitor->socket < 0) {
        free(monitor);
        return nullptr;
    }
    
    // Set socket timeout (100ms)
    int timeout_us = 100000;  // 100ms in microseconds
    sceNetSetsockopt(monitor->socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, 
                     &timeout_us, sizeof(timeout_us));
    
    // Bind to port
    SceNetSockaddrIn addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = SCE_NET_AF_INET;
    addr.sin_port = sceNetHtons(port);
    addr.sin_addr.s_addr = SCE_NET_INADDR_ANY;
    
    if (sceNetBind(monitor->socket, (SceNetSockaddr*)&addr, sizeof(addr)) < 0) {
        sceNetSocketClose(monitor->socket);
        free(monitor);
        return nullptr;
    }
    
    // Initialize state
    monitor->status = MOONMIC_DISCONNECTED;
    monitor->last_ping_time = 0;
    monitor->running = 1;
    monitor->paused = 0;  // Start unpaused
    
    // Create monitor thread
    monitor->thread_id = sceKernelCreateThread("heartbeat_mon", monitor_thread_func, 
                                               0x10000100, 0x4000, 0, 0, nullptr);
    if (monitor->thread_id >= 0) {
        // Pass monitor pointer to thread
        sceKernelStartThread(monitor->thread_id, sizeof(heartbeat_monitor_t*), &monitor);
    }
    
    return monitor;
}

void heartbeat_monitor_destroy(heartbeat_monitor_t* monitor) {
    if (!monitor) {
        return;
    }
    
    monitor->running = 0;
    
    // Wait for thread to exit
    if (monitor->thread_id >= 0) {
        sceKernelWaitThreadEnd(monitor->thread_id, nullptr, nullptr);
    }
    
    sceNetSocketClose(monitor->socket);
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
    return monitor->paused != 0;
}

} // extern "C"
