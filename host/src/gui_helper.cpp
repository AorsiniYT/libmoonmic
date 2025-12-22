/**
 * @file gui_helper.cpp
 * @brief Helper functions for ImGui tooltips implementation
 */

#include "gui_helper.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace moonmic {

void ShowHelpTooltip(const char* text) {
#ifdef USE_IMGUI
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
#endif
}

void ShowDetailedTooltip(const char* title, const char* description) {
#ifdef USE_IMGUI
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        
        // Title in bold yellow
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", title);
        ImGui::Separator();
        ImGui::Spacing();
        
        // Description
        ImGui::TextUnformatted(description);
        
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
#endif
}

// Tooltip constant definitions
namespace Tooltips {
    const char* VIRTUAL_MIC = 
        "Enable virtual microphone mode.\n"
        "When enabled, the virtual mic (Steam/VBCable) becomes the system default.\n"
        "Your original mic will be restored when the app closes.";
    
    const char* ORIGINAL_MIC_SELECTOR = 
        "Select which microphone to restore when the app closes.\n"
        "This is your 'physical' microphone that will become the default\n"
        "when you exit moonmic-host or if the app crashes unexpectedly.\n\n"
        "The guardian watchdog ensures restoration even on crashes.";
    
    const char* CURRENT_DEFAULT_MIC = 
        "Shows the currently active default recording device.\n"
        "This is what Windows applications will use for microphone input.";
    
    const char* VBCABLE_DRIVER = 
        "VB-Audio Virtual Cable driver.\n"
        "Provides a virtual audio device for routing audio between applications.\n"
        "Supports WASAPI and WDM-KS modes.";
    
    const char* STEAM_DRIVER = 
        "Steam Streaming Microphone driver.\n"
        "Low-latency virtual microphone designed for game streaming.\n"
        "Optimized for WDM-KS with minimal overhead.";
    
    const char* INSTALL_DRIVER = 
        "Install the selected virtual audio driver.\n"
        "Requires administrator privileges.\n"
        "A system reboot is recommended after installation.";
    
    const char* UNINSTALL_DRIVER = 
        "Remove the virtual audio driver from the system.\n"
        "The app will automatically stop audio playback before uninstalling\n"
        "to prevent conflicts. Requires administrator privileges.";
    
    const char* AUDIO_RECEIVER_STATUS = 
        "Shows the current state of the audio receiver.\n"
        "Running: Listening for audio from Sunshine client\n"
        "Stopped: Not receiving audio";
    
    const char* SAMPLE_RATE = 
        "Audio sample rate in Hz.\n"
        "48000 Hz is recommended for best quality.\n"
        "Lower rates (16000 Hz) may be required for some virtual devices.";
    
    const char* BUFFER_SIZE = 
        "Audio buffer size affects latency and stability.\n"
        "Smaller buffers = lower latency but may cause audio glitches.\n"
        "Larger buffers = more stable but higher latency.";
    
    const char* SUNSHINE_AUTO_CONFIG = 
        "Automatically configure Sunshine when connected.\n"
        "Updates host IP and audio settings for optimal streaming.";
    
    const char* SUNSHINE_WEBUI = 
        "Open Sunshine's web interface in your browser.\n"
        "Allows you to configure advanced streaming settings.";
    
    const char* GUARDIAN_STATUS = 
        "Guardian watchdog status.\n"
        "Active: Monitoring for crashes, will restore original mic if needed\n"
        "Inactive: Not monitoring (guardian disabled or failed to start)";
    
    const char* DEBUG_MODE = 
        "Enable debug mode for detailed logging.\n"
        "Shows packet statistics, connection info, and internal events.\n"
        "Useful for troubleshooting connection issues.";
    
    const char* PACKET_STATS = 
        "Real-time packet statistics.\n"
        "Received: Total packets successfully received\n"
        "Dropped: Packets lost due to network issues\n"
        "Lag: Packets dropped due to processing delays";
}

} // namespace moonmic
