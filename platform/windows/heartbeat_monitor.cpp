/**
 * @file heartbeat_monitor.cpp
 * @brief Windows heartbeat monitor implementation
 */

#include "../heartbeat_monitor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstring>
#include <cstdlib>

#pragma comment(lib, "ws2_32.lib")

#define PING_MAGIC 0x50494E47  // "PING"
#define PING_TIMEOUT_MS 3000   // 3 seconds

#pragma pack(push, 1)
struct ping_packet {
    uint32_t magic;
    uint64_t timestamp;
};
#pragma pack(pop)

struct heartbeat_monitor_t {
    SOCKET socket;
    volatile LONG running;
    volatile moonmic_connection_status_t status;
    volatile ULONGLONG last_ping_time;
    HANDLE thread_handle;
};

// Get time in milliseconds
static ULONGLONG get_time_ms() {
    return GetTickCount64();
}

// Monitor thread function
static DWORD WINAPI monitor_thread_func(LPVOID param) {
    heartbeat_monitor_t* monitor = (heartbeat_monitor_t*)param;
    ping_packet ping;
    
    while (InterlockedCompareExchange(&monitor->running, 0, 0)) {
        // Receive with timeout
        int received = recv(monitor->socket, (char*)&ping, sizeof(ping), 0);
        
        if (received == sizeof(ping) && ping.magic == PING_MAGIC) {
            // Valid PING received
            monitor->last_ping_time = get_time_ms();
            monitor->status = MOONMIC_CONNECTED;
        }
        
        // Check for timeout
        ULONGLONG now = get_time_ms();
        if (now - monitor->last_ping_time > PING_TIMEOUT_MS) {
            monitor->status = MOONMIC_DISCONNECTED;
        }
}
    
    return 0;
}

extern "C" {

heartbeat_monitor_t* heartbeat_monitor_create(uint16_t port) {
    // Initialize Winsock
    static bool wsa_initialized = false;
    if (!wsa_initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return nullptr;
        }
        wsa_initialized = true;
    }
    
    heartbeat_monitor_t* monitor = (heartbeat_monitor_t*)calloc(1, sizeof(heartbeat_monitor_t));
    if (!monitor) {
        return nullptr;
    }
    
    // Create UDP socket
    monitor->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (monitor->socket == INVALID_SOCKET) {
        free(monitor);
        return nullptr;
    }
    
    // Set socket timeout (100ms)
    DWORD timeout_ms = 100;
    setsockopt(monitor->socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
    
    // Bind to port
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(monitor->socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(monitor->socket);
        free(monitor);
        return nullptr;
    }
    
    // Initialize state
    monitor->status = MOONMIC_DISCONNECTED;
    monitor->last_ping_time = 0;
    InterlockedExchange(&monitor->running, 1);
    
    // Create monitor thread
    monitor->thread_handle = CreateThread(nullptr, 0, monitor_thread_func, monitor, 0, nullptr);
    if (!monitor->thread_handle) {
        closesocket(monitor->socket);
        free(monitor);
        return nullptr;
    }
    
    return monitor;
}

void heartbeat_monitor_destroy(heartbeat_monitor_t* monitor) {
    if (!monitor) {
        return;
    }
    
    InterlockedExchange(&monitor->running, 0);
    
    // Wait for thread to exit
    if (monitor->thread_handle) {
        WaitForSingleObject(monitor->thread_handle, INFINITE);
        CloseHandle(monitor->thread_handle);
    }
    
    closesocket(monitor->socket);
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

} // extern "C"
