/**
 * @file guardian_launcher.cpp
 * @brief Guardian process launcher implementation (Single-EXE architecture)
 */

#include "guardian_launcher.h"
#include "guardian_state.h"
#include <iostream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#endif

namespace moonmic {

const char* GuardianLauncher::SHUTDOWN_EVENT_NAME = "Global\\MoonMicHostShutdown";

bool GuardianLauncher::launchGuardian(const std::string& original_mic_id, 
                                      const std::string& original_mic_name) {
    // Save state first
    GuardianState state;
    state.original_mic_id = original_mic_id;
    state.original_mic_name = original_mic_name;
#ifdef _WIN32
    state.host_pid = GetCurrentProcessId();
#else
    state.host_pid = getpid();
#endif
    state.timestamp = time(nullptr);
    
    if (!GuardianStateManager::writeState(state)) {
        std::cerr << "[GuardianLauncher] Failed to write state" << std::endl;
        return false;
    }

#ifdef _WIN32
    // Create shutdown event
    HANDLE hEvent = CreateEventA(NULL, TRUE, FALSE, SHUTDOWN_EVENT_NAME);
    if (!hEvent) {
        std::cerr << "[GuardianLauncher] Failed to create shutdown event" << std::endl;
        return false;
    }
    CloseHandle(hEvent);
    
    // Get guardian path next to moonmic-host.exe
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::filesystem::path guardianPath = std::filesystem::path(exePath).parent_path() / "moonmic-guardian.exe";
    
    if (!std::filesystem::exists(guardianPath)) {
        std::cerr << "[GuardianLauncher] Guardian executable not found: " << guardianPath << std::endl;
        return false;
    }
    
    // Launch guardian
    std::string cmdLine = "\"" + guardianPath.string() + "\" " + std::to_string(GetCurrentProcessId());
    
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    if (!CreateProcessA(NULL, const_cast<char*>(cmdLine.c_str()),
                        NULL, NULL, FALSE,
                        CREATE_NO_WINDOW | DETACHED_PROCESS,
                        NULL, NULL, &si, &pi)) {
        std::cerr << "[GuardianLauncher] Failed to launch guardian" << std::endl;
        return false;
    }
    
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    
    std::cout << "[GuardianLauncher] Guardian launched (PID: " << pi.dwProcessId << ")" << std::endl;
    return true;
#else
    // Linux implementation
    std::string exePath = std::filesystem::canonical("/proc/self/exe");
    std::filesystem::path guardianPath = std::filesystem::path(exePath).parent_path() / "moonmic-guardian";

    if (!std::filesystem::exists(guardianPath)) {
        std::cerr << "[GuardianLauncher] Guardian executable not found: " << guardianPath << std::endl;
        return false;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        std::string pidStr = std::to_string(getpid());
        execl(guardianPath.c_str(), guardianPath.c_str(), pidStr.c_str(), NULL);
        // If execl returns, it failed
        exit(1);
    } else if (pid > 0) {
        // Parent process
        std::cout << "[GuardianLauncher] Guardian launched (PID: " << pid << ")" << std::endl;
        return true;
    } else {
        std::cerr << "[GuardianLauncher] fork() failed" << std::endl;
        return false;
    }
#endif
}

void GuardianLauncher::signalNormalShutdown() {
#ifdef _WIN32
    HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, SHUTDOWN_EVENT_NAME);
    if (hEvent) {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
    // Give guardian time to process signal
    Sleep(100);
#else
    // On Linux we could use a signal, but common state deletion might be enough
    // since the guardian checks if process is alive.
    // For now, just deleting the state is sufficient.
#endif
    
    // Clean up state file
    GuardianStateManager::deleteState();
}

bool GuardianLauncher::isGuardianRunning() {
    return GuardianStateManager::stateExists();
}

} // namespace moonmic
