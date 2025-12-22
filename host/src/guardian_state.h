/**
 * @file guardian_state.h
 * @brief Guardian state file management for crash recovery
 */

#pragma once

#include <string>
#include <ctime>

namespace moonmic {

/**
 * @brief State information for guardian process
 */
struct GuardianState {
    std::string original_mic_id;      // Device GUID
    std::string original_mic_name;    // Friendly name
    unsigned long host_pid;           // Main process PID
    time_t timestamp;                 // When state was saved
    
    GuardianState() : host_pid(0), timestamp(0) {}
};

/**
 * @brief Guardian state file manager
 */
class GuardianStateManager {
public:
    /**
     * @brief Get path to guardian state file
     * @return Absolute path to moonmic.state
     */
    static std::string getStatePath();
    
    /**
     * @brief Write guardian state to file
     * @param state State to save
     * @return true on success
     */
    static bool writeState(const GuardianState& state);
    
    /**
     * @brief Read guardian state from file
     * @param state Output parameter for state
     * @return true if state was read successfully
     */
    static bool readState(GuardianState& state);
    
    /**
     * @brief Delete guardian state file
     */
    static void deleteState();
    
    /**
     * @brief Check if state file exists
     * @return true if moonmic.state exists
     */
    static bool stateExists();
};

} // namespace moonmic
