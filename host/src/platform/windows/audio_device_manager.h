/**
 * @file audio_device_manager.h
 * @brief Audio device management for Windows (IPolicyConfig)
 */

#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

namespace moonmic {

// Audio device info structure
struct AudioDeviceInfo {
    std::string id;           // Device ID (GUID)
    std::string name;         // Friendly name
    bool is_default;          // Is current default recording device
    bool is_virtual;          // Is virtual device (Steam/VBCable)
};

// IPolicyConfig interface (undocumented Windows API for changing default device)
// This interface is used by the Windows Sound Control Panel
// We define it without DECLSPEC_UUID for MinGW compatibility
interface IPolicyConfig : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, void*, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, REFPROPERTYKEY, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, REFPROPERTYKEY, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR wszDeviceId, ERole role) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

// IID and CLSID for IPolicyConfig (Windows 10+)
static const IID IID_IPolicyConfig = 
    {0xf8679f50, 0x850a, 0x41cf, {0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8}};

// CLSID for CPolicyConfigClient (Windows 10+)
static const CLSID CLSID_CPolicyConfigClient = 
    {0x870af99c, 0x171d, 0x4f9e, {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}};

class AudioDeviceManager {
public:
    AudioDeviceManager();
    ~AudioDeviceManager();

    /**
     * @brief Enumerate all recording (capture) devices
     * @return Vector of device info
     */
    std::vector<AudioDeviceInfo> enumerateRecordingDevices();

    /**
     * @brief Get current default recording device
     * @return Device info, or empty if error
     */
    AudioDeviceInfo getCurrentDefaultRecordingDevice();

    /**
     * @brief Set default recording device by ID
     * @param device_id Device GUID
     * @return true on success
     */
    bool setDefaultRecordingDevice(const std::string& device_id);

    /**
     * @brief Check if a device is a virtual microphone (Steam/VBCable)
     * @param device_name Friendly name
     * @return true if virtual
     */
    static bool isVirtualMicrophone(const std::string& device_name);

private:
    IMMDeviceEnumerator* enumerator_ = nullptr;
    IPolicyConfig* policy_config_ = nullptr;

    bool initializeCOM();
    void cleanupCOM();
};

} // namespace moonmic
