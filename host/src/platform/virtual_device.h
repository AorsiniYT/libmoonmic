/**
 * @file virtual_device.h
 * @brief Virtual audio device interface
 */

#pragma once

#include <string>
#include <memory>

namespace moonmic {

class VirtualDevice {
public:
    virtual ~VirtualDevice() = default;
    
    virtual bool init(const std::string& device_name, int sample_rate, int channels) = 0;
    virtual bool write(const float* data, size_t frames, int channels) = 0;
    virtual void close() = 0;
    virtual int getSampleRate() const = 0;  // Get actual device sample rate
    
    // Returns buffer usage fraction (0.0 to 1.0). Default 0.0 for non-buffered devices.
    virtual float getBufferUsage() const { return 0.0f; }
    
    static std::unique_ptr<VirtualDevice> create();
};

} // namespace moonmic
