/**
 * @file single_instance.cpp
 * @brief Single instance implementation
 */

#include "single_instance.h"
#include <iostream>

#ifdef _WIN32
// Windows implementation
#else
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace moonmic {

SingleInstance::SingleInstance(const std::string& app_name)
#ifdef _WIN32
    : mutex_(NULL)
    , window_title_(app_name)
#else
    : lock_fd_(-1)
    , lock_file_("/tmp/" + app_name + ".lock")
#endif
{
#ifdef _WIN32
    // Create a named mutex
    std::string mutex_name = "Global\\MoonMicHost_Mutex";
    mutex_ = CreateMutexA(NULL, FALSE, mutex_name.c_str());
#else
    // Create lock file
    lock_fd_ = open(lock_file_.c_str(), O_CREAT | O_RDWR, 0666);
    if (lock_fd_ != -1) {
        flock(lock_fd_, LOCK_EX | LOCK_NB);
    }
#endif
}

SingleInstance::~SingleInstance() {
#ifdef _WIN32
    if (mutex_) {
        ReleaseMutex(mutex_);
        CloseHandle(mutex_);
    }
#else
    if (lock_fd_ != -1) {
        flock(lock_fd_, LOCK_UN);
        close(lock_fd_);
        unlink(lock_file_.c_str());
    }
#endif
}

bool SingleInstance::isAnotherInstanceRunning() {
#ifdef _WIN32
    if (mutex_ == NULL) {
        return false;
    }
    
    DWORD result = WaitForSingleObject(mutex_, 0);
    if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) {
        // We got the mutex, no other instance
        return false;
    }
    
    // If we failed to get the mutex, try waiting a bit (race condition fix)
    // The old instance might be in the process of closing
    std::cout << "[SingleInstance] Mutex held, waiting for release..." << std::endl;
    for (int i = 0; i < 10; i++) {
        Sleep(200); // Wait 200ms
        result = WaitForSingleObject(mutex_, 0);
        if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) {
            std::cout << "[SingleInstance] Acquired mutex after wait" << std::endl;
            return false;
        }
    }
    
    // Another instance is definitely running
    return true;
#else
    if (lock_fd_ == -1) {
        return false;
    }
    
    int result = flock(lock_fd_, LOCK_EX | LOCK_NB);
    if (result == 0) {
        // We got the lock, no other instance
        return false;
    }
    
    // Another instance is running
    return true;
#endif
}

void SingleInstance::bringExistingToFront() {
#ifdef _WIN32
    // Find the window by title
    HWND hwnd = FindWindowA(NULL, window_title_.c_str());
    if (hwnd) {
        // Restore if minimized
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        }
        
        // Bring to front
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);
        
        std::cout << "[SingleInstance] Brought existing window to front" << std::endl;
    }
#else
    // On Linux, we'd need to use X11 or implement IPC
    std::cout << "[SingleInstance] Another instance is already running" << std::endl;
#endif
}

} // namespace moonmic
