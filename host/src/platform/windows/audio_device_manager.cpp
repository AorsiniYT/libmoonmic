/**
 * @file audio_device_manager.cpp
 * @brief Audio device management implementation
 */

#include <initguid.h>
#include "audio_device_manager.h"
#include <iostream>
#include <propidl.h>
#include <comdef.h>

namespace moonmic {

AudioDeviceManager::AudioDeviceManager() {
    initializeCOM();
}

AudioDeviceManager::~AudioDeviceManager() {
    cleanupCOM();
}

bool AudioDeviceManager::initializeCOM() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "[AudioDeviceManager] CoInitialize failed" << std::endl;
        return false;
    }

    // Create device enumerator
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&enumerator_);
    if (FAILED(hr)) {
        std::cerr << "[AudioDeviceManager] Failed to create device enumerator" << std::endl;
        return false;
    }

    // Create policy config (for changing default device)
    hr = CoCreateInstance(CLSID_CPolicyConfigClient, NULL, CLSCTX_ALL,
                          IID_IPolicyConfig, (void**)&policy_config_);
    if (FAILED(hr)) {
        std::cerr << "[AudioDeviceManager] Failed to create IPolicyConfig (Windows 10+ required)" << std::endl;
        // Non-fatal, device enumeration still works
    }

    return true;
}

void AudioDeviceManager::cleanupCOM() {
    if (policy_config_) {
        policy_config_->Release();
        policy_config_ = nullptr;
    }
    if (enumerator_) {
        enumerator_->Release();
        enumerator_ = nullptr;
    }
    CoUninitialize();
}

std::vector<AudioDeviceInfo> AudioDeviceManager::enumerateRecordingDevices() {
    std::vector<AudioDeviceInfo> devices;

    if (!enumerator_) {
        std::cerr << "[AudioDeviceManager] Enumerator not initialized" << std::endl;
        return devices;
    }

    // Get default device ID for comparison
    IMMDevice* defaultDevice = nullptr;
    std::wstring defaultId;
    HRESULT hr = enumerator_->GetDefaultAudioEndpoint(eCapture, eConsole, &defaultDevice);
    if (SUCCEEDED(hr)) {
        LPWSTR pwszID = NULL;
        defaultDevice->GetId(&pwszID);
        if (pwszID) {
            defaultId = pwszID;
            CoTaskMemFree(pwszID);
        }
        defaultDevice->Release();
    }

    // Enumerate all capture devices
    IMMDeviceCollection* collection = nullptr;
    hr = enumerator_->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) {
        std::cerr << "[AudioDeviceManager] EnumAudioEndpoints failed" << std::endl;
        return devices;
    }

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; i++) {
        IMMDevice* device = nullptr;
        hr = collection->Item(i, &device);
        if (FAILED(hr)) continue;

        AudioDeviceInfo info;

        // Get device ID
        LPWSTR pwszID = NULL;
        device->GetId(&pwszID);
        if (pwszID) {
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, pwszID, -1, NULL, 0, NULL, NULL);
            std::string id(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, pwszID, -1, &id[0], size_needed, NULL, NULL);
            info.id = id.c_str(); // Remove null terminator
            info.is_default = (defaultId == pwszID);
            CoTaskMemFree(pwszID);
        }

        // Get friendly name
        IPropertyStore* props = nullptr;
        device->OpenPropertyStore(STGM_READ, &props);
        if (props) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            props->GetValue(PKEY_Device_FriendlyName, &varName);
            if (varName.vt == VT_LPWSTR) {
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, NULL, 0, NULL, NULL);
                std::string name(size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, &name[0], size_needed, NULL, NULL);
                info.name = name.c_str();
                info.is_virtual = isVirtualMicrophone(info.name);
            }
            PropVariantClear(&varName);
            props->Release();
        }

        device->Release();
        devices.push_back(info);
    }

    collection->Release();
    return devices;
}

AudioDeviceInfo AudioDeviceManager::getCurrentDefaultRecordingDevice() {
    AudioDeviceInfo info;

    if (!enumerator_) return info;

    IMMDevice* device = nullptr;
    HRESULT hr = enumerator_->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
    if (FAILED(hr)) return info;

    // Get device ID
    LPWSTR pwszID = NULL;
    device->GetId(&pwszID);
    if (pwszID) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, pwszID, -1, NULL, 0, NULL, NULL);
        std::string id(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, pwszID, -1, &id[0], size_needed, NULL, NULL);
        info.id = id.c_str();
        CoTaskMemFree(pwszID);
    }

    // Get friendly name
    IPropertyStore* props = nullptr;
    device->OpenPropertyStore(STGM_READ, &props);
    if (props) {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        props->GetValue(PKEY_Device_FriendlyName, &varName);
        if (varName.vt == VT_LPWSTR) {
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, NULL, 0, NULL, NULL);
            std::string name(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, &name[0], size_needed, NULL, NULL);
            info.name = name.c_str();
            info.is_virtual = isVirtualMicrophone(info.name);
        }
        PropVariantClear(&varName);
        props->Release();
    }

    info.is_default = true;
    device->Release();
    return info;
}

bool AudioDeviceManager::setDefaultRecordingDevice(const std::string& device_id) {
    if (!policy_config_) {
        std::cerr << "[AudioDeviceManager] IPolicyConfig not available" << std::endl;
        return false;
    }

    // Convert UTF-8 to wide string
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, NULL, 0);
    std::wstring wid(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, &wid[0], size_needed);

    // Set as default for all roles (Console, Multimedia, Communications)
    HRESULT hr = policy_config_->SetDefaultEndpoint(wid.c_str(), eConsole);
    if (FAILED(hr)) {
        std::cerr << "[AudioDeviceManager] SetDefaultEndpoint failed: " << hr << std::endl;
        return false;
    }

    policy_config_->SetDefaultEndpoint(wid.c_str(), eMultimedia);
    policy_config_->SetDefaultEndpoint(wid.c_str(), eCommunications);

    std::cout << "[AudioDeviceManager] Changed default recording device" << std::endl;
    return true;
}

bool AudioDeviceManager::isVirtualMicrophone(const std::string& device_name) {
    // Check for known virtual microphone patterns
    return (device_name.find("Steam") != std::string::npos && 
            device_name.find("Microphone") != std::string::npos) ||
           (device_name.find("CABLE Output") != std::string::npos) ||
           (device_name.find("VB-Audio") != std::string::npos && 
            device_name.find("Output") != std::string::npos);
}

} // namespace moonmic
