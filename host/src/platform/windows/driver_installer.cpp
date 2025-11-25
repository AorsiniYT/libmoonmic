/**
 * @file driver_installer.cpp
 * @brief VB-CABLE driver installer implementation
 */

#include "driver_installer.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <filesystem>

namespace moonmic {

DriverInstaller::DriverInstaller() {
    // Get driver path relative to executable
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    std::filesystem::path exe_dir = std::filesystem::path(exe_path).parent_path();
    driver_path_ = (exe_dir / "driver").string();
}

bool DriverInstaller::isRunningAsAdmin() {
    BOOL is_admin = FALSE;
    PSID admin_group = NULL;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    
    if (AllocateAndInitializeSid(&nt_authority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &admin_group)) {
        
        CheckTokenMembership(NULL, admin_group, &is_admin);
        FreeSid(admin_group);
    }
    
    return is_admin == TRUE;
}

bool DriverInstaller::restartAsAdmin() {
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    
    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.lpVerb = "runas";
    sei.lpFile = exe_path;
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;
    
    if (!ShellExecuteExA(&sei)) {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            std::cerr << "[DriverInstaller] User cancelled UAC prompt" << std::endl;
        }
        return false;
    }
    
    return true;
}

bool DriverInstaller::isVBCableInstalled() {
    // Check if CABLE Input device exists
    std::string input = getVBCableInputDevice();
    std::string output = getVBCableOutputDevice();
    
    return !input.empty() && !output.empty();
}

std::string DriverInstaller::getVBCableInputDevice() {
    HRESULT hr;
    IMMDeviceEnumerator* enumerator = NULL;
    IMMDeviceCollection* collection = NULL;
    
    CoInitialize(NULL);
    
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&enumerator
    );
    
    if (FAILED(hr)) {
        CoUninitialize();
        return "";
    }
    
    // Enumerate render devices (playback)
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) {
        enumerator->Release();
        CoUninitialize();
        return "";
    }
    
    UINT count;
    collection->GetCount(&count);
    
    std::string result;
    
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
                    
                    // Look for "CABLE Input" (VB-CABLE playback device)
                    if (name.find("CABLE Input") != std::string::npos) {
                        result = name;
                    }
                }
                
                PropVariantClear(&var_name);
                props->Release();
            }
            
            device->Release();
        }
        
        if (!result.empty()) break;
    }
    
    collection->Release();
    enumerator->Release();
    CoUninitialize();
    
    return result;
}

std::string DriverInstaller::getVBCableOutputDevice() {
    HRESULT hr;
    IMMDeviceEnumerator* enumerator = NULL;
    IMMDeviceCollection* collection = NULL;
    
    CoInitialize(NULL);
    
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&enumerator
    );
    
    if (FAILED(hr)) {
        CoUninitialize();
        return "";
    }
    
    // Enumerate capture devices (recording)
    hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) {
        enumerator->Release();
        CoUninitialize();
        return "";
    }
    
    UINT count;
    collection->GetCount(&count);
    
    std::string result;
    
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
                    
                    // Look for "CABLE Output" (VB-CABLE virtual microphone)
                    if (name.find("CABLE Output") != std::string::npos) {
                        result = name;
                    }
                }
                
                PropVariantClear(&var_name);
                props->Release();
            }
            
            device->Release();
        }
        
        if (!result.empty()) break;
    }
    
    collection->Release();
    enumerator->Release();
    CoUninitialize();
    
    return result;
}

bool DriverInstaller::installDriver() {
    if (!isRunningAsAdmin()) {
        std::cerr << "[DriverInstaller] Administrator privileges required" << std::endl;
        return false;
    }
    
    return runSetupExecutable();
}

bool DriverInstaller::runSetupExecutable() {
    // Determine architecture
    SYSTEM_INFO sys_info;
    GetNativeSystemInfo(&sys_info);
    
    std::string setup_exe;
    if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
        setup_exe = driver_path_ + "\\VBCABLE_Setup_x64.exe";
    } else {
        setup_exe = driver_path_ + "\\VBCABLE_Setup.exe";
    }
    
    // Check if setup exists
    if (!std::filesystem::exists(setup_exe)) {
        std::cerr << "[DriverInstaller] Setup not found: " << setup_exe << std::endl;
        return false;
    }
    
    std::cout << "[DriverInstaller] Running: " << setup_exe << std::endl;
    
    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = "open";
    sei.lpFile = setup_exe.c_str();
    sei.nShow = SW_SHOW;
    
    if (!ShellExecuteExA(&sei)) {
        std::cerr << "[DriverInstaller] Failed to run setup" << std::endl;
        return false;
    }
    
    // Wait for setup to complete
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        
        DWORD exit_code;
        GetExitCodeProcess(sei.hProcess, &exit_code);
        CloseHandle(sei.hProcess);
        
        if (exit_code == 0) {
            std::cout << "[DriverInstaller] Installation completed successfully" << std::endl;
            std::cout << "[DriverInstaller] Please reboot your computer to finalize installation" << std::endl;
            return true;
        } else {
            std::cerr << "[DriverInstaller] Installation failed with code: " << exit_code << std::endl;
            return false;
        }
    }
    
    return false;
}

} // namespace moonmic
