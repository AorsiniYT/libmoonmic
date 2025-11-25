/**
 * @file main.cpp
 * @brief Main application entry point for moonmic-host
 */

#include "config.h"
#include "audio_receiver.h"
#include "sunshine_integration.h"
#include "single_instance.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include "platform/windows/driver_installer.h"
#endif

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

// STB Image for loading window icon
#define STB_IMAGE_IMPLEMENTATION
#include "platform/windows/stb_image.h"
#endif

using namespace moonmic;

static bool g_running = true;
static AudioReceiver* g_receiver = nullptr;

void signal_handler(int signal) {
    std::cout << "\n[Main] Shutting down..." << std::endl;
    g_running = false;
}

#ifdef USE_IMGUI

void renderGUI(GLFWwindow* window, AudioReceiver& receiver, SunshineIntegration& sunshine, Config& config) {
    // Make the ImGui window fill the entire GLFW window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    window_flags |= ImGuiWindowFlags_NoNavFocus;
    
    ImGui::Begin("MoonMic Host", nullptr, window_flags);
    
    // Add title manually
    ImGui::Text("MoonMic host by AorsiniYT - v1.0.0");
    ImGui::Separator();
    
    // Status
    ImGui::Text("Status: %s", receiver.isRunning() ? "Running" : "Stopped");
    ImGui::Separator();
    
#ifdef _WIN32
    // VB-CABLE Driver Status
    ImGui::Text("VB-CABLE Driver");
    DriverInstaller installer;
    bool vbcable_installed = installer.isVBCableInstalled();
    
    if (vbcable_installed) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "[OK] VB-CABLE Installed");
        std::string input_device = installer.getVBCableInputDevice();
        std::string output_device = installer.getVBCableOutputDevice();
        if (!input_device.empty()) {
            ImGui::Text("  Input: %s", input_device.c_str());
        }
        if (!output_device.empty()) {
            ImGui::Text("  Output: %s", output_device.c_str());
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "[!] VB-CABLE Not Installed");
        ImGui::TextWrapped("VB-CABLE virtual audio driver is required for audio routing.");
        ImGui::Spacing();
        
        if (ImGui::Button("Install VB-CABLE Driver", ImVec2(200, 30))) {
            if (!DriverInstaller::isRunningAsAdmin()) {
                ImGui::OpenPopup("Need Admin");
            } else {
                if (installer.installDriver()) {
                    ImGui::OpenPopup("Install Success");
                } else {
                    ImGui::OpenPopup("Install Failed");
                }
            }
        }
        
        // Admin privileges popup
        if (ImGui::BeginPopupModal("Need Admin", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Administrator privileges required.");
            ImGui::Text("The application will restart with admin rights.");
            ImGui::Separator();
            
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                if (DriverInstaller::restartAsAdmin()) {
                    // Successfully restarted as admin, exit this instance
                    std::cout << "[Main] Restarting as admin, closing current instance..." << std::endl;
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        // Installation success popup
        if (ImGui::BeginPopupModal("Install Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("VB-CABLE driver installed successfully!");
            ImGui::Text("Please reboot your computer to complete installation.");
            ImGui::Separator();
            
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        // Installation failed popup
        if (ImGui::BeginPopupModal("Install Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Failed to install VB-CABLE driver.");
            ImGui::Text("Please check the console for error details.");
            ImGui::Separator();
            
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    
    ImGui::Separator();
#endif
    
    // Sunshine Integration
    ImGui::Text("Sunshine Integration");
    if (sunshine.isSunshineDetected()) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "[OK] Sunshine Detected");
        
        const auto& clients = sunshine.getPairedClients();
        ImGui::Text("Paired Clients: %zu", clients.size());
        
        if (ImGui::TreeNode("Client List")) {
            for (const auto& client : clients) {
                ImGui::BulletText("%s (%s)", client.name.c_str(), client.uuid.c_str());
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "[X] Sunshine Not Detected");
    }
    
    ImGui::Separator();
    
    // Statistics
    auto stats = receiver.getStats();
    ImGui::Text("Statistics");
    ImGui::Text("Packets Received: %llu", stats.packets_received);
    ImGui::Text("Packets Dropped: %llu", stats.packets_dropped);
    ImGui::Text("Bytes Received: %llu", stats.bytes_received);
    ImGui::Text("Last Sender: %s", stats.last_sender_ip.c_str());
    ImGui::Text("Receiving: %s", stats.is_receiving ? "Yes" : "No");
    
    ImGui::Separator();
    
    // Configuration
    ImGui::Text("Configuration");
    ImGui::Text("Port: %d", config.server.port);
    ImGui::Text("Sample Rate: %d Hz", config.audio.sample_rate);
    ImGui::Text("Channels: %d", config.audio.channels);
    
    // Whitelist checkbox with auto-save
    bool prev_whitelist = config.security.enable_whitelist;
    if (ImGui::Checkbox("Whitelist Enabled", &config.security.enable_whitelist)) {
        if (prev_whitelist != config.security.enable_whitelist) {
            // Configuration changed, auto-save
            std::string config_path = Config::getDefaultConfigPath();
            if (config.save(config_path)) {
                std::cout << "[Config] Auto-saved changes" << std::endl;
            }
        }
    }
    
    ImGui::Separator();
    
    // Controls
    if (receiver.isRunning()) {
        if (ImGui::Button("Stop Receiver")) {
            receiver.stop();
        }
    } else {
        if (ImGui::Button("Start Receiver")) {
            receiver.start(config);
        }
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Reload Sunshine")) {
        sunshine.reload();
        
        // Update allowed_clients in config with Sunshine paired clients
        if (sunshine.isSunshineDetected()) {
            const auto& paired_clients = sunshine.getPairedClients();
            config.security.allowed_clients.clear();
            for (const auto& client : paired_clients) {
                config.security.allowed_clients.push_back(client.uuid);
            }
            
            // Auto-save updated client list
            std::string config_path = Config::getDefaultConfigPath();
            if (config.save(config_path)) {
                std::cout << "[Config] Auto-saved Sunshine client list" << std::endl;
            }
        }
    }
    
    ImGui::End();
}

int main_gui(int argc, char* argv[]) {
    // Check for single instance
    SingleInstance single_instance("MoonMic host by AorsiniYT");
    if (single_instance.isAnotherInstanceRunning()) {
        std::cout << "[Main] Another instance is already running" << std::endl;
        single_instance.bringExistingToFront();
        return 0;
    }
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }
    
    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(600, 500, "MoonMic host by AorsiniYT", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // VSync
    
#ifdef _WIN32
    // Set window icon from embedded PNG resource (Windows only)
    HRSRC hResource = FindResourceA(NULL, "IDR_WINDOW_ICON_PNG", RT_RCDATA);
    if (hResource) {
        HGLOBAL hLoadedResource = LoadResource(NULL, hResource);
        if (hLoadedResource) {
            LPVOID pResourceData = LockResource(hLoadedResource);
            DWORD dwResourceSize = SizeofResource(NULL, hResource);
            
            if (pResourceData && dwResourceSize > 0) {
                int icon_width, icon_height, icon_channels;
                unsigned char* icon_pixels = stbi_load_from_memory(
                    (const unsigned char*)pResourceData, 
                    dwResourceSize, 
                    &icon_width, 
                    &icon_height, 
                    &icon_channels, 
                    4
                );
                
                if (icon_pixels) {
                    GLFWimage icon;
                    icon.width = icon_width;
                    icon.height = icon_height;
                    icon.pixels = icon_pixels;
                    
                    glfwSetWindowIcon(window, 1, &icon);
                    stbi_image_free(icon_pixels);
                    std::cout << "[Main] Window icon set successfully" << std::endl;
                } else {
                    std::cerr << "[Main] Failed to decode icon from resource" << std::endl;
                }
            }
        }
    } else {
        std::cerr << "[Main] Failed to find icon resource" << std::endl;
    }
#endif
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Load configuration
    Config config;
    std::string config_path = Config::getDefaultConfigPath();
    if (!config.load(config_path)) {
        std::cout << "[Main] Using default configuration" << std::endl;
    }
    
    // Create components
    AudioReceiver receiver;
    SunshineIntegration sunshine;
    
    // Auto-start if configured
    if (config.gui.show_on_startup) {
        receiver.start(config);
    }
    
    g_receiver = &receiver;
    
    // Main loop
    while (!glfwWindowShouldClose(window) && g_running) {
        glfwPollEvents();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        renderGUI(window, receiver, sunshine, config);
        
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
    receiver.stop();
    
    // Auto-save configuration on exit
    std::cout << "[Main] Saving configuration on exit..." << std::endl;
    config.save(config_path);
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}

#endif // USE_IMGUI

int main_console(int argc, char* argv[]) {
    std::cout << "=== MoonMic Host ===" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl << std::endl;
    
    // Check for --install-driver first
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--install-driver") {
#ifdef _WIN32
            std::cout << "[Main] Installing VB-CABLE driver..." << std::endl;
            DriverInstaller installer;
            
            if (!DriverInstaller::isRunningAsAdmin()) {
                std::cout << "[Main] Requesting administrator privileges..." << std::endl;
                if (DriverInstaller::restartAsAdmin()) {
                    return 0; // Exit this instance, admin instance will run
                } else {
                    std::cerr << "[Main] Failed to restart as administrator" << std::endl;
                    return 1;
                }
            }
            
            if (installer.installDriver()) {
                std::cout << "[Main] Driver installation completed" << std::endl;
                std::cout << "[Main] Please reboot your computer" << std::endl;
                return 0;
            } else {
                std::cerr << "[Main] Driver installation failed" << std::endl;
                return 1;
            }
#else
            std::cerr << "[Main] --install-driver is only supported on Windows" << std::endl;
            return 1;
#endif
        }
    }
    
    // Load configuration
    Config config;
    std::string config_path = Config::getDefaultConfigPath();
    
    // Check for --config argument
    for (int i = 1; i < argc - 1; i++) {
        if (std::string(argv[i]) == "--config") {
            config_path = argv[i + 1];
            break;
        }
    }
    
    if (!config.load(config_path)) {
        std::cout << "[Main] Using default configuration" << std::endl;
    }
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create audio receiver
    AudioReceiver receiver;
    g_receiver = &receiver;
    
    // Start receiver
    if (!receiver.start(config)) {
        std::cerr << "[Main] Failed to start receiver" << std::endl;
        return 1;
    }
    
    std::cout << "[Main] Press Ctrl+C to stop" << std::endl;
    
    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Print stats periodically
        auto stats = receiver.getStats();
        if (stats.is_receiving) {
            std::cout << "[Stats] Packets: " << stats.packets_received 
                      << " | Dropped: " << stats.packets_dropped
                      << " | From: " << stats.last_sender_ip << std::endl;
        }
    }
    
    receiver.stop();
    std::cout << "[Main] Shutdown complete" << std::endl;
    
    return 0;
}

int main(int argc, char* argv[]) {
    // Immediate output to verify program starts
    std::cout << "moonmic-host starting..." << std::endl;
    std::cout.flush();
    
    try {
#ifdef USE_IMGUI
        // Check if --no-gui flag is present
        bool use_gui = true;
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--no-gui") {
                use_gui = false;
                break;
            }
        }
        
        std::cout << "GUI mode: " << (use_gui ? "enabled" : "disabled") << std::endl;
        
        if (use_gui) {
            return main_gui(argc, argv);
        }
#endif
        
        return main_console(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}
