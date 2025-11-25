/**
 * @file udp_receiver.cpp
 * @brief UDP receiver implementation
 */

#include "udp_receiver.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

namespace moonmic {

#ifdef _WIN32
static DWORD WINAPI thread_func(LPVOID arg) {
    auto* receiver = static_cast<UDPReceiver*>(arg);
    receiver->receiveLoop();
    return 0;
}
#else
static void* thread_func(void* arg) {
    auto* receiver = static_cast<UDPReceiver*>(arg);
    receiver->receiveLoop();
    return nullptr;
}
#endif

UDPReceiver::UDPReceiver()
    : socket_fd_(INVALID_SOCKET)
    , running_(false)
    , thread_handle_(nullptr) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

UDPReceiver::~UDPReceiver() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool UDPReceiver::start(int port, const std::string& bind_address) {
    if (running_) {
        return false;
    }
    
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd_ == INVALID_SOCKET) {
        std::cerr << "[UDPReceiver] Failed to create socket" << std::endl;
        return false;
    }
    
    // Set socket options
    int reuse = 1;
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
    
    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(bind_address.c_str());
    
    if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[UDPReceiver] Failed to bind to port " << port << std::endl;
        closesocket(socket_fd_);
        socket_fd_ = INVALID_SOCKET;
        return false;
    }
    
    running_ = true;
    
    // Start receiver thread
#ifdef _WIN32
    thread_handle_ = CreateThread(NULL, 0, thread_func, this, 0, NULL);
#else
    pthread_t* thread = new pthread_t;
    pthread_create(thread, NULL, thread_func, this);
    thread_handle_ = thread;
#endif
    
    std::cout << "[UDPReceiver] Started on " << bind_address << ":" << port << std::endl;
    return true;
}

void UDPReceiver::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (socket_fd_ != INVALID_SOCKET) {
        closesocket(socket_fd_);
        socket_fd_ = INVALID_SOCKET;
    }
    
    if (thread_handle_) {
#ifdef _WIN32
        WaitForSingleObject(thread_handle_, INFINITE);
        CloseHandle(thread_handle_);
#else
        pthread_t* thread = static_cast<pthread_t*>(thread_handle_);
        pthread_join(*thread, NULL);
        delete thread;
#endif
        thread_handle_ = nullptr;
    }
    
    std::cout << "[UDPReceiver] Stopped" << std::endl;
}

void UDPReceiver::receiveLoop() {
    uint8_t buffer[4096];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    
    while (running_) {
        int received = recvfrom(
            socket_fd_,
            (char*)buffer,
            sizeof(buffer),
            0,
            (struct sockaddr*)&sender_addr,
            &sender_len
        );
        
        if (received < 0) {
            if (running_) {
                std::cerr << "[UDPReceiver] Receive error" << std::endl;
            }
            break;
        }
        
        if (received < sizeof(PacketHeader)) {
            continue;  // Packet too small
        }
        
        // Verify magic number
        PacketHeader* header = (PacketHeader*)buffer;
        if (header->magic != MOONMIC_MAGIC) {
            continue;  // Invalid packet
        }
        
        // Get sender IP
        char sender_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
        
        // Extract Opus data (after header)
        const uint8_t* opus_data = buffer + sizeof(PacketHeader);
        size_t opus_size = received - sizeof(PacketHeader);
        
        // Call callback
        if (packet_callback_) {
            packet_callback_(opus_data, opus_size, std::string(sender_ip));
        }
    }
}

} // namespace moonmic
