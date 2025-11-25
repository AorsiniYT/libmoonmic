/**
 * @file main.cpp
 * @brief Main application entry point for moonmic-host
 */

#include "config.h"
#include "audio_receiver.h"
#include "sunshine_integration.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#endif

using namespace moonmic;

static bool g_running = true;
static AudioReceiver* g_receiver = nullptr;

void signal_handler(int signal) {
    std::cout << "\n[Main] Shutting down..." << std::endl;
    g_running = false;
}

#ifdef USE_IMGUI

void renderGUI(AudioReceiver& receiver, SunshineIntegration& sunshine, Config& config) {
    ImGui::Begin("MoonMic Host", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    // Status
    ImGui::Text("Status: %s", receiver.isRunning() ? "Running" : "Stopped");
    ImGui::Separator();
    
    // Sunshine Integration
    ImGui::Text("Sunshine Integration");
    if (sunshine.detectSunshine()) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Sunshine Detected");
        
        auto clients = sunshine.loadPairedClients();
        ImGui::Text("Paired Clients: %zu", clients.size());
        
        if (ImGui::TreeNode("Client List")) {
            for (const auto& client : clients) {
                ImGui::BulletText("%s (%s)", client.name.c_str(), client.uuid.c_str());
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Sunshine Not Detected");
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
    ImGui::Checkbox("Whitelist Enabled", &config.security.enable_whitelist);
    
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
    }
    
    ImGui::End();
}

int main_gui(int argc, char* argv[]) {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }
    
    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(600, 500, "MoonMic Host", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // VSync
    
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
        
        renderGUI(receiver, sunshine, config);
        
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
#ifdef USE_IMGUI
    // Check if --no-gui flag is present
    bool use_gui = true;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--no-gui") {
            use_gui = false;
            break;
        }
    }
    
    if (use_gui) {
        return main_gui(argc, argv);
    }
#endif
    
    return main_console(argc, argv);
}
