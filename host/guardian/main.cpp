/**
 * @file main.cpp  
 * @brief moonmic-guardian - Standalone watchdog executable with ImGui interface
 */

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include "../src/guardian_state.h"

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
            ImGui::TextWrapped("Guardian is attempting to restore your original microphone settings.");
            ImGui::Spacing();
            if (ImGui::Button("Restore Now", ImVec2(200, 40))) {
                if (platform::restoreMicrophone(ui.state.original_mic_id)) {
                    ui.mode = GuardianMode::RESULT_SUCCESS;
                    ui.message = "Microphone restored successfully!";
                    GuardianStateManager::deleteState();
                } else {
                    ui.mode = GuardianMode::RESULT_FAILED;
                    ui.message = "Automatic restoration failed. Please check your settings.";
                }
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

    // Loop
    while (!glfwWindowShouldClose(window) && !ui.shouldExit) {
        // Monitoring Logic
        if (ui.mode == GuardianMode::MONITORING) {
            if (!platform::isProcessAlive(targetPid)) {
                // Host died!
                
#ifdef _WIN32
                // Check if it was a normal shutdown
                bool normalShutdown = false;
                HANDLE hEvent = OpenEventA(SYNCHRONIZE, FALSE, "Global\\MoonMicHostShutdown");
                if (hEvent) {
                    if (WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0) {
                        normalShutdown = true;
                    }
                    CloseHandle(hEvent);
                }

                if (normalShutdown) {
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
