/**
 * @file virtual_device_win.cpp
 * @brief Windows virtual audio device using VB-CABLE
 */


#include "../virtual_device.h"
#include "driver_installer.h"
#define INIT GUID  // Define GUIDs in this compilation unit
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>

namespace moonmic {

class VirtualDeviceWindows : public VirtualDevice {
public:
    VirtualDeviceWindows() 
        : audio_client_(nullptr)
        , render_client_(nullptr)
        , buffer_frame_count_(0) {
        // Initialize COM - ignore RPC_E_CHANGED_MODE if already initialized with different mode
        HRESULT hr = CoInitialize(NULL);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            std::cerr << "[VirtualDevice] Failed to initialize COM: 0x" << std::hex << hr << std::endl;
        }
    }
    
    ~VirtualDeviceWindows() override {
        close();
        CoUninitialize();
    }
    
    bool init(const std::string& device_name, int sample_rate, int channels) override {
        HRESULT hr;
        
        // Check if VB-CABLE is installed
        DriverInstaller installer;
        std::string vb_cable_input = installer.getVBCableInputDevice();
        
        if (vb_cable_input.empty()) {
            std::cerr << "[VirtualDevice] VB-CABLE not detected" << std::endl;
            std::cerr << "[VirtualDevice] Please install VB-CABLE from driver/ folder" << std::endl;
            std::cerr << "[VirtualDevice] Or run: moonmic-host --install-driver" << std::endl;
            return false;
        }
        
        std::cout << "[VirtualDevice] Found VB-CABLE: " << vb_cable_input << std::endl;
        
        // Get VB-CABLE Input device (this is where we write audio)
        IMMDevice* device = findDeviceByName(vb_cable_input);
        if (!device) {
            std::cerr << "[VirtualDevice] Failed to open VB-CABLE Input" << std::endl;
            return false;
        }
        
        // Activate audio client
        hr = device->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            NULL,
            (void**)&audio_client_
        );
        device->Release();
        
        if (FAILED(hr)) {
            std::cerr << "[VirtualDevice] Failed to activate audio client" << std::endl;
            return false;
        }
        
        // Get mix format
        WAVEFORMATEX* format = nullptr;
        hr = audio_client_->GetMixFormat(&format);
        if (FAILED(hr)) {
            audio_client_->Release();
            audio_client_ = nullptr;
            return false;
        }
        
        // Initialize audio client
        hr = audio_client_->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0,
            10000000,  // 1 second buffer
            0,
            format,
            NULL
        );
        
        CoTaskMemFree(format);
        
        if (FAILED(hr)) {
            audio_client_->Release();
            audio_client_ = nullptr;
            return false;
        }
        
        // Get buffer size
        hr = audio_client_->GetBufferSize(&buffer_frame_count_);
        if (FAILED(hr)) {
            audio_client_->Release();
            audio_client_ = nullptr;
            return false;
        }
        
        // Get render client
        hr = audio_client_->GetService(
            __uuidof(IAudioRenderClient),
            (void**)&render_client_
        );
        
        if (FAILED(hr)) {
            audio_client_->Release();
            audio_client_ = nullptr;
            return false;
        }
        
        // Start audio client
        hr = audio_client_->Start();
        if (FAILED(hr)) {
            render_client_->Release();
            audio_client_->Release();
            render_client_ = nullptr;
            audio_client_ = nullptr;
            return false;
        }
        
        std::cout << "[VirtualDevice] Initialized with VB-CABLE Input" << std::endl;
        std::cout << "[VirtualDevice] Virtual microphone: CABLE Output" << std::endl;
        return true;
    }
    
    bool write(const float* data, size_t frames, int channels) override {
        if (!render_client_) {
            return false;
        }
        
        UINT32 padding;
        HRESULT hr = audio_client_->GetCurrentPadding(&padding);
        if (FAILED(hr)) {
            return false;
        }
        
        UINT32 available = buffer_frame_count_ - padding;
        if (available < frames) {
            frames = available;
        }
        
        if (frames == 0) {
            return true;
        }
        
        BYTE* buffer;
        hr = render_client_->GetBuffer(frames, &buffer);
        if (FAILED(hr)) {
            return false;
        }
        
        memcpy(buffer, data, frames * channels * sizeof(float));
        
        hr = render_client_->ReleaseBuffer(frames, 0);
        return SUCCEEDED(hr);
    }
    
    void close() override {
        if (audio_client_) {
            audio_client_->Stop();
        }
        
        if (render_client_) {
            render_client_->Release();
            render_client_ = nullptr;
        }
        
        if (audio_client_) {
            audio_client_->Release();
            audio_client_ = nullptr;
        }
    }
    
private:
    IAudioClient* audio_client_;
    IAudioRenderClient* render_client_;
    UINT32 buffer_frame_count_;
    
    IMMDevice* findDeviceByName(const std::string& target_name) {
        IMMDeviceEnumerator* enumerator = NULL;
        IMMDeviceCollection* collection = NULL;
        
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            NULL,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            (void**)&enumerator
        );
        
        if (FAILED(hr)) {
            std::cerr << "[VirtualDevice] Failed to create device enumerator" << std::endl;
            return NULL;
        }
        
        hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
        if (FAILED(hr)) {
            std::cerr << "[VirtualDevice] Failed to enumerate audio endpoints" << std::endl;
            enumerator->Release();
            return NULL;
        }
        
        UINT count;
        collection->GetCount(&count);
        
        std::cout << "[VirtualDevice] Searching for device: '" << target_name << "'" << std::endl;
        std::cout << "[VirtualDevice] Found " << count << " render devices" << std::endl;
        
        IMMDevice* result = NULL;
        
        for (UINT i = 0; i < count; i++) {
            IMMDevice* device = NULL;
            collection->Item(i, &device);
            
            if (device) {
                IPropertyStore* props = NULL;
                device->OpenPropertyStore(STGM_READ, &props);
                
                if (props) {
                    PROPVARIANT var_name;
                    PropVariantInit(&var_name);
                    
                    props->GetValue(PKEY_Device_FriendlyName, &var_name);
                    
                    if (var_name.vt == VT_LPWSTR) {
                        std::wstring wname(var_name.pwszVal);
                        std::string name(wname.begin(), wname.end());
                        
                        std::cout << "[VirtualDevice]   Device " << i << ": '" << name << "'" << std::endl;
                        
                        // Try exact match first, then substring match
                        if (name == target_name || name.find(target_name) != std::string::npos) {
                            std::cout << "[VirtualDevice] MATCH! Using device: '" << name << "'" << std::endl;
                            result = device;
                            result->AddRef();
                        }
                    }
                    
                    PropVariantClear(&var_name);
                    props->Release();
                }
                
                device->Release();
            }
            
            if (result) break;
        }
        
        if (!result) {
            std::cerr << "[VirtualDevice] No matching device found for: '" << target_name << "'" << std::endl;
        }
        
        collection->Release();
        enumerator->Release();
        
        return result;
    }
};

std::unique_ptr<VirtualDevice> VirtualDevice::create() {
    return std::make_unique<VirtualDeviceWindows>();
}

} // namespace moonmic
