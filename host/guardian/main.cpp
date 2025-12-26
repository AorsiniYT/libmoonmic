/**
 * @file main.cpp  
 * @brief moonmic-guardian - Standalone watchdog executable with ImGui interface
 */

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <iomanip>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include "../src/guardian_state.h"
#include "../src/config.h"
#include "../src/logger.h"
#include "../src/guardian_launcher.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include "../src/platform/windows/audio_device_manager.h"
#else
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

using namespace moonmic;

// --- Platform Abstractions ---

namespace platform {
    bool isProcessAlive(unsigned long pid) {
#ifdef _WIN32
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess) return false;
        DWORD exitCode;
        if (GetExitCodeProcess(hProcess, &exitCode)) {
            CloseHandle(hProcess);
            return exitCode == STILL_ACTIVE;
        }
        CloseHandle(hProcess);
        return false;
#else
        return (kill(pid, 0) == 0);
#endif
    }

    bool restoreMicrophone(const std::string& micId) {
#ifdef _WIN32
        AudioDeviceManager devMgr;
        return devMgr.setDefaultRecordingDevice(micId);
#else
        // TODO: Linux PulseAudio/PipeWire restoration
        std::cout << "[Guardian] Restoration not yet implemented on Linux" << std::endl;
        return false;
#endif
    }

    std::string getCurrentMicName() {
#ifdef _WIN32
        AudioDeviceManager devMgr;
        return devMgr.getCurrentDefaultRecordingDevice().name;
#else
        return "Unknown (Linux)";
#endif
    }
}

// --- UI State ---

enum class GuardianMode {
    MONITORING,
    TEST_MODE,
    CRASH_DETECTED,
    RESULT_SUCCESS,
    RESULT_FAILED
};

struct UIState {
    GuardianMode mode = GuardianMode::MONITORING;
    GuardianState state;
    std::string message;
    bool shouldExit = false;
    bool windowVisible = false;
};

std::string getLogPath() {
    return Logger::getLogPath();
}

void dumpLog(UIState& ui) {
    std::string logPath = getLogPath();
    if (!std::filesystem::exists(logPath)) {
        ui.message = "Log file not found: " + logPath;
        ui.mode = GuardianMode::RESULT_FAILED;
        return;
    }
    
    auto now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "moonmic_crash_%Y%m%d_%H%M%S.log", std::localtime(&now));
    std::string dumpName = buf;
    
    std::filesystem::path p(logPath);
    std::string dumpPath = (p.parent_path() / dumpName).string();
    
    try {
        std::filesystem::copy_file(logPath, dumpPath, std::filesystem::copy_options::overwrite_existing);
        ui.message = "Log dumped to:\n" + dumpPath;
        ui.mode = GuardianMode::RESULT_SUCCESS;
    } catch (const std::exception& e) {
        ui.message = "Failed to dump log: " + std::string(e.what());
        ui.mode = GuardianMode::RESULT_FAILED;
    }
}

void viewLog() {
    std::string logPath = getLogPath();
#ifdef _WIN32
    // Open a CMD window to display the log content
    // /K keeps the window open so the user can read the output
    std::string params = "/K echo [MoonMic Log Viewer] && echo File: " + logPath + " && echo ---------------------------------------- && type \"" + logPath + "\"";
    ShellExecuteA(NULL, "open", "cmd.exe", params.c_str(), NULL, SW_SHOW);
#else
    std::string cmd = "xdg-open \"" + logPath + "\"";
    system(cmd.c_str());
#endif
}

void renderUI(UIState& ui) {
    if (!ui.windowVisible) return;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | 
                            ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("Guardian", nullptr, flags);

    switch (ui.mode) {
        case GuardianMode::TEST_MODE:
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "MoonMic Guardian - Test Mode");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextWrapped("This is the watchdog process. It normally runs in the background.");
            ImGui::TextWrapped("Click 'Test Restore' to verify if your microphone can be restored to its original state.");
            ImGui::Spacing();
            if (ImGui::Button("Test Restore", ImVec2(120, 40))) {
                if (GuardianStateManager::readState(ui.state)) {
                    if (platform::restoreMicrophone(ui.state.original_mic_id)) {
                        ui.mode = GuardianMode::RESULT_SUCCESS;
                        ui.message = "Microphone restored to: " + ui.state.original_mic_name;
                        GuardianStateManager::deleteState();
                    } else {
                        ui.mode = GuardianMode::RESULT_FAILED;
                        ui.message = "Failed to restore microphone.";
                    }
                } else {
                    ui.mode = GuardianMode::RESULT_FAILED;
                    ui.message = "No saved microphone state found. Run moonmic-host first.";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Exit", ImVec2(120, 40))) {
                ui.shouldExit = true;
            }
            break;

        case GuardianMode::CRASH_DETECTED:
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "MoonMic Host Crashed!");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextWrapped("The main application closed unexpectedly.");
            ImGui::Spacing();
            ImGui::TextWrapped("%s", ui.message.c_str());
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            if (ImGui::Button("Dump Log", ImVec2(140, 30))) {
                dumpLog(ui);
            }
            ImGui::SameLine();
            if (ImGui::Button("View Log", ImVec2(140, 30))) {
                viewLog();
            }
            
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 40))) {
                ui.shouldExit = true;
            }
            break;

        case GuardianMode::RESULT_SUCCESS:
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Success");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextWrapped("%s", ui.message.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 40))) {
                ui.shouldExit = true;
            }
            break;

        case GuardianMode::RESULT_FAILED:
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Restoration Failed");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextWrapped("%s", ui.message.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 40))) {
                ui.shouldExit = true;
            }
            break;

        default:
            ImGui::Text("Monitoring...");
            break;
    }

    ImGui::End();
}

int main(int argc, char* argv[]) {
    UIState ui;
    unsigned long targetPid = 0;

    // Parse arguments
    if (argc >= 2) {
        try {
            targetPid = std::stoul(argv[1]);
            ui.mode = GuardianMode::MONITORING;
            ui.windowVisible = false;
        } catch (...) {
            ui.mode = GuardianMode::TEST_MODE;
            ui.windowVisible = true;
        }
    } else {
        ui.mode = GuardianMode::TEST_MODE;
        ui.windowVisible = true;
    }

    // Initialize GLFW
    if (!glfwInit()) return 1;

    // Window hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (!ui.windowVisible) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    GLFWwindow* window = glfwCreateWindow(400, 300, "MoonMic Guardian", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    ImGui::StyleColorsDark();

    // Open synchronization events
    std::cout << "[Guardian] Watchdog active. Using Local Events for sync." << std::endl;
    
    // Guardian should be robust against missing events (app might have crashed before creating them) even after host exits
#ifdef _WIN32
    // Use the Shared Constants to ensure Host and Guardian speak the same language
    HANDLE hShutdownEvent = OpenEventA(SYNCHRONIZE, FALSE, GuardianLauncher::SHUTDOWN_EVENT_NAME);
    HANDLE hRestartEvent = OpenEventA(SYNCHRONIZE, FALSE, GuardianLauncher::RESTART_EVENT_NAME);
    
    if (!hShutdownEvent) {
         // If open failed, maybe it doesn't exist yet (Host startup race?). 
         // But usually Host creates it before launching Guardian.
         // If failed due to permissions (Global vs Local), this confirms the issue.
         // We try to create/open using the constant.
         hShutdownEvent = CreateEventA(NULL, TRUE, FALSE, GuardianLauncher::SHUTDOWN_EVENT_NAME);
    }
    if (!hRestartEvent) {
         hRestartEvent = CreateEventA(NULL, TRUE, FALSE, GuardianLauncher::RESTART_EVENT_NAME);
    }
#endif

    // Loop
    while (!glfwWindowShouldClose(window) && !ui.shouldExit) {
        // Monitoring Logic
        if (ui.mode == GuardianMode::MONITORING) {
            if (!platform::isProcessAlive(targetPid)) {
                // Host died!
                
#ifdef _WIN32
                bool restartRequested = false;
                bool normalShutdown = false;
                
                // Check for Restart Signal
                if (hRestartEvent) {
                    if (WaitForSingleObject(hRestartEvent, 0) == WAIT_OBJECT_0) {
                        restartRequested = true;
                    }
                }
                
                // Check for Normal Shutdown Signal
                if (hShutdownEvent) {
                    if (WaitForSingleObject(hShutdownEvent, 0) == WAIT_OBJECT_0) {
                        normalShutdown = true;
                    }
                }

                if (restartRequested) {
                    // Restart requested: Relaunch host and exit guardian
                    std::cout << "[Guardian] Restart requested. Relaunching host..." << std::endl;
                    
                    // Get host path (we are in same dir)
                    char exePath[MAX_PATH];
                    GetModuleFileNameA(NULL, exePath, MAX_PATH);
                    std::filesystem::path hostPath = std::filesystem::path(exePath).parent_path() / "moonmic-host.exe";
                    
                    ShellExecuteA(NULL, "open", hostPath.string().c_str(), NULL, NULL, SW_SHOW);
                    ui.shouldExit = true;
                    
                } else if (normalShutdown) {
                    // Normal exit: Restore silently if needed and exit
                    if (GuardianStateManager::readState(ui.state)) {
                        // Restore silently
                        platform::restoreMicrophone(ui.state.original_mic_id);
                        GuardianStateManager::deleteState();
                    }
                    ui.shouldExit = true;
                } else {
                    // Crash detected: Read state and show window
                    if (GuardianStateManager::readState(ui.state)) {
                        ui.mode = GuardianMode::CRASH_DETECTED;
                        
                        // Auto-restore immediately
                        if (platform::restoreMicrophone(ui.state.original_mic_id)) {
                            ui.message = "Microphone restored automatically.";
                            GuardianStateManager::deleteState();
                        } else {
                            ui.message = "Automatic restoration failed.";
                        }
                        
                        ui.windowVisible = true;
                        glfwShowWindow(window);
                    } else {
                        // No state to restore, just exit
                        ui.shouldExit = true;
                    }
                }
#else
                // Linux: Just check state for now
                if (GuardianStateManager::readState(ui.state)) {
                    ui.mode = GuardianMode::CRASH_DETECTED;
                    ui.windowVisible = true;
                    glfwShowWindow(window);
                } else {
                    ui.shouldExit = true;
                }
#endif
            }
        }

        // Event polling
        if (ui.windowVisible) {
            glfwPollEvents();
        } else {
            // Background mode: check every 500ms
            glfwWaitEventsTimeout(0.5);
        }

        // Render
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderUI(ui);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
