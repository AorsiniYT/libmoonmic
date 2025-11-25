/**
 * @file driver_installer.cpp
 * @brief VB-CABLE driver installer implementation
 */


#include "driver_installer.h"
#define INITGUID
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

bool DriverInstaller::extractResourceToFile(const char* resource_name, const std::string& output_path) {
    // Find the embedded resource
    // Use RT_RCDATA (standard resource type 10) instead of "RCDATA" string
    HRSRC hResource = FindResourceA(NULL, resource_name, RT_RCDATA);
    if (!hResource) {
        std::cerr << "[DriverInstaller] Resource not found: " << resource_name << std::endl;
        return false;
    }
    
    // Load and lock the resource
    HGLOBAL hLoadedResource = LoadResource(NULL, hResource);
    if (!hLoadedResource) {
        return false;
    }
    
    LPVOID pResourceData = LockResource(hLoadedResource);
    if (!pResourceData) {
        return false;
    }
    
    DWORD dwResourceSize = SizeofResource(NULL, hResource);
    if (dwResourceSize == 0) {
        return false;
    }
    
    // Write to file
    HANDLE hFile = CreateFileA(
        output_path.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "[DriverInstaller] Failed to create: " << output_path << std::endl;
        return false;
    }
    
    DWORD dwBytesWritten;
    BOOL bResult = WriteFile(hFile, pResourceData, dwResourceSize, &dwBytesWritten, NULL);
    CloseHandle(hFile);
    
    return (bResult && dwBytesWritten == dwResourceSize);
}

bool DriverInstaller::extractEmbeddedInstaller(const std::string& temp_dir) {
    std::cout << "[DriverInstaller] Extracting embedded VB-CABLE driver files..." << std::endl;
    
    // Create temp directory
    std::filesystem::create_directories(temp_dir);
    
    // Resource mappings: {resource_name, output_filename}
    struct ResourceFile {
        const char* resource;
        const char* filename;
    };
    
    ResourceFile files[] = {
        // Installers
        {"IDR_VBCABLE_SETUP_X64", "VBCABLE_Setup_x64.exe"},
        {"IDR_VBCABLE_SETUP", "VBCABLE_Setup.exe"},
        {"IDR_VBCABLE_CONTROLPANEL", "VBCABLE_ControlPanel.exe"},
        
        // Windows 10/11 x64
        {"IDR_DRIVER_WIN10_INF", "vbMmeCable64_win10.inf"},
        {"IDR_DRIVER_WIN10_SYS", "vbaudio_cable64_win10.sys"},
        {"IDR_DRIVER_WIN10_CAT", "vbaudio_cable64_win10.cat"},
        {"IDR_DRIVER_WIN10_ARM_SYS", "vbaudio_cable64arm_win10.sys"},
        
        // Windows 7 x64
        {"IDR_DRIVER_WIN7_64_INF", "vbMmeCable64_win7.inf"},
        {"IDR_DRIVER_WIN7_64_SYS", "vbaudio_cable64_win7.sys"},
        {"IDR_DRIVER_WIN7_64_CAT", "vbaudio_cable64_win7.cat"},
        
        // Windows Vista x64
        {"IDR_DRIVER_VISTA_64_INF", "vbMmeCable64_vista.inf"},
        {"IDR_DRIVER_VISTA_64_SYS", "vbaudio_cable64_vista.sys"},
        {"IDR_DRIVER_VISTA_64_CAT", "vbaudio_cable64_vista.cat"},
        
        // Windows 2003 x64
        {"IDR_DRIVER_2003_64_INF", "vbMmeCable64_2003.inf"},
        {"IDR_DRIVER_2003_64_SYS", "vbaudio_cable64_2003.sys"},
        {"IDR_DRIVER_2003_64_CAT", "vbaudio_cable64_2003.cat"},
        
        // Windows 7 x86
        {"IDR_DRIVER_WIN7_32_INF", "vbMmeCable_win7.inf"},
        {"IDR_DRIVER_WIN7_32_SYS", "vbaudio_cable_win7.sys"},
        {"IDR_DRIVER_WIN7_32_CAT", "vbaudio_cable_win7.cat"},
        
        // Windows Vista x86
        {"IDR_DRIVER_VISTA_32_INF", "vbMmeCable_vista.inf"},
        {"IDR_DRIVER_VISTA_32_SYS", "vbaudio_cable_vista.sys"},
        {"IDR_DRIVER_VISTA_32_CAT", "vbaudio_cable_vista.cat"},
        
        // Windows XP
        {"IDR_DRIVER_XP_INF", "vbMmeCable_xp.inf"},
        {"IDR_DRIVER_XP_SYS", "vbaudio_cable_xp.sys"},
        {"IDR_DRIVER_XP_CAT", "vbaudio_cable_xp.cat"},
        
        // Windows 2003 x86
        {"IDR_DRIVER_2003_32_INF", "vbMmeCable_2003.inf"},
        {"IDR_DRIVER_2003_32_SYS", "vbaudio_cable_2003.sys"},
        {"IDR_DRIVER_2003_32_CAT", "vbaudio_cable_2003.cat"},
        
        // Extras
        {"IDR_ICON_PIN_IN", "pin_in.ico"},
        {"IDR_ICON_PIN_OUT", "pin_out.ico"},
        {"IDR_README_TXT", "readme.txt"}
    };
    
    int extracted = 0;
    for (const auto& file : files) {
        std::string output_path = temp_dir + "\\" + file.filename;
        if (extractResourceToFile(file.resource, output_path)) {
            extracted++;
        }
    }
    
    std::cout << "[DriverInstaller] Extracted " << extracted << " files to: " << temp_dir << std::endl;
    return extracted > 0;
}

void DriverInstaller::cleanupTempDir(const std::string& temp_dir) {
    try {
        if (std::filesystem::exists(temp_dir)) {
            std::filesystem::remove_all(temp_dir);
            std::cout << "[DriverInstaller] Cleaned up temporary files" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DriverInstaller] Failed to cleanup temp dir: " << e.what() << std::endl;
    }
}

bool DriverInstaller::runSetupExecutable() {
    // Determine architecture
    SYSTEM_INFO sys_info;
    GetNativeSystemInfo(&sys_info);
    
    std::string setup_exe;
    std::string temp_dir_path;
    bool use_embedded = false;
    
    if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
        setup_exe = driver_path_ + "\\VBCABLE_Setup_x64.exe";
    } else {
        setup_exe = driver_path_ + "\\VBCABLE_Setup.exe";
    }
    
    // Check if setup exists in driver folder
    if (!std::filesystem::exists(setup_exe)) {
        std::cout << "[DriverInstaller] Driver folder not found, using embedded files" << std::endl;
        
        // Create temp directory for driver files
        char temp_path[MAX_PATH];
        GetTempPathA(MAX_PATH, temp_path);
        temp_dir_path = std::string(temp_path) + "moonmic_vbcable";
        
        // Extract all driver files
        if (!extractEmbeddedInstaller(temp_dir_path)) {
            std::cerr << "[DriverInstaller] Failed to extract embedded files" << std::endl;
            return false;
        }
        
        // Update setup_exe path to temp directory
        if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
            setup_exe = temp_dir_path + "\\VBCABLE_Setup_x64.exe";
        } else {
            setup_exe = temp_dir_path + "\\VBCABLE_Setup.exe";
        }
        
        use_embedded = true;
    }
    
    std::cout << "[DriverInstaller] Running: " << setup_exe << std::endl;
    
    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = "open";
    sei.lpFile = setup_exe.c_str();
    sei.nShow = SW_SHOW;
    
    if (!ShellExecuteExA(&sei)) {
        std::cerr << "[DriverInstaller] Failed to run setup" << std::endl;
        if (use_embedded) {
            cleanupTempDir(temp_dir_path);
        }
        return false;
    }
    
    // Wait for setup to complete
    bool success = false;
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        
        DWORD exit_code;
        GetExitCodeProcess(sei.hProcess, &exit_code);
        CloseHandle(sei.hProcess);
        
        if (exit_code == 0) {
            std::cout << "[DriverInstaller] Installation completed successfully" << std::endl;
            std::cout << "[DriverInstaller] Please reboot your computer to finalize installation" << std::endl;
            success = true;
        } else {
            std::cerr << "[DriverInstaller] Installation failed with code: " << exit_code << std::endl;
        }
    }
    
    // Clean up extracted files
    if (use_embedded) {
        cleanupTempDir(temp_dir_path);
    }
    
    return success;
}

} // namespace moonmic
