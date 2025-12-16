/**
 * @file udp_receiver.h
 * @brief UDP receiver for moonmic packets
 */

#pragma once

#include <stdint.h>
#include <functional>
#include <string>

namespace moonmic {

#define MOONMIC_MAGIC 0x4D4D4943

struct PacketHeader {
    uint32_t magic;
    uint32_t sequence;
    uint64_t timestamp;
};

class UDPReceiver {
public:
    using PacketCallback = std::function<void(const uint8_t* data, size_t size, const std::string& sender_ip, uint16_t sender_port, bool is_lagging)>;
    
    UDPReceiver();
    ~UDPReceiver();
    
    bool start(int port, const std::string& bind_address = "0.0.0.0");
    void stop();
    bool isRunning() const { return running_; }
    
    void setPacketCallback(PacketCallback callback) { packet_callback_ = callback; }
    
    void receiveLoop();
    
    // Send packet from the bound socket (thread-safe)
    bool sendTo(const void* data, size_t size, const std::string& ip, uint16_t port);
    
private:
    
#ifdef _WIN32
    using socket_t = unsigned long long; // SOCKET type on Windows x64
#else
    using socket_t = int; // POSIX socket type
#endif
    
    socket_t socket_fd_;
    bool running_;
    void* thread_handle_;
    PacketCallback packet_callback_;
};

} // namespace moonmic
