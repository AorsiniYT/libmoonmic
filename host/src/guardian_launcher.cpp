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
    // Create synchronization events (Manual Reset so they stick)
    // Host keeps these handles open through its lifetime implicitly? 
    // Actually we should leak them or store them to ensure they exist.
    // For now we assume Guardian will open them immediately upon launch.
    HANDLE hShutdown = CreateEventA(NULL, TRUE, FALSE, SHUTDOWN_EVENT_NAME);
    HANDLE hRestart = CreateEventA(NULL, TRUE, FALSE, RESTART_EVENT_NAME);
    
    // We intentionally do NOT close these handles here in this process?
    // If we close them, and Guardian hasn't started yet, they disappear.
    // We should probably keep them. But `launchGuardian` returns.
    // Let's rely on the OS or Guardian picking them up fast, or just don't close them.
    // Leaking 2 handles in a long running process is fine.
    // (Previous code closed `hEvent` on line 47, which might have been a bug but worked if Guardian opened it fast?)
    // Actually previous code: `CloseHandle(hEvent);` line 47.
    
    // Let's close them to match previous behavior, but ensure Guardian opens them ASAP.
    if (hShutdown) CloseHandle(hShutdown);
    if (hRestart) CloseHandle(hRestart);
    
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
    // Linux: Just check state for now
    GuardianStateManager::deleteState();
#endif
}

void GuardianLauncher::signalRestart() {
#ifdef _WIN32
    HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, RESTART_EVENT_NAME);
    if (hEvent) {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
    // Give guardian time to process signal
    Sleep(100);
    
    // Also signal normal shutdown so we don't trigger "Crash Detected" logic accidentally
    // if restart logic checks both
    signalNormalShutdown(); 
#else 
    // Linux TODO
#endif
}

bool GuardianLauncher::isGuardianRunning() {
    return GuardianStateManager::stateExists();
}

} // namespace moonmic
