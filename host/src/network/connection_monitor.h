#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <thread>

namespace moonmic {

// Ping packet structure
#pragma pack(push, 1)
struct MoonMicPing {
    uint32_t magic;      // 0x50494E47 ("PING")
    uint64_t timestamp;  // Microseconds since epoch
};
#pragma pack(pop)

/**
 * @brief Connection monitor that sends periodic pings to clients
 */
class ConnectionMonitor {
public:
    ConnectionMonitor();
    ~ConnectionMonitor();
    
    /**
     * Start sending pings to the specified client
     * @param client_ip IP address of the client
     * @param port UDP port to send pings to
     */
    void start(const std::string& client_ip, uint16_t port);
    
    /**
     * Stop sending pings
     */
    void stop();
    
    /**
     * Check if monitor is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * Send arbitrary packet to the client
     * @param data Packet data to send
     * @param size Size of the packet in bytes
     */
    void sendPacket(const void* data, size_t size);
    
private:
    void pingThreadFunc();
    
    std::atomic<bool> running_;
    std::string client_ip_;
    uint16_t client_port_;
    std::thread ping_thread_;
    int socket_fd_;
};

} // namespace moonmic
