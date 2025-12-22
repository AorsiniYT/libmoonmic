/**
 * @file audio_utils.h
 * @brief Windows Audio Utilities for managing default devices
 */

#pragma once

#include <string>
#include <vector>

namespace moonmic {
namespace platform {
namespace windows {

/**
 * @brief Get the ID and Friendly Name of the current default recording device (Console role)
 * @param deviceId Output: The device ID string
 * @param friendlyName Output: The device friendly name
 * @return true if successful
 */
bool GetDefaultRecordingDevice(std::string& deviceId, std::string& friendlyName);

/**
 * @brief Set the default recording device by name or ID (Console & Communications roles)
 * @param nameOrId The device ID or part of the friendly name to match
 * @return true if successful
 */
bool SetDefaultRecordingDevice(const std::string& nameOrId);

/**
 * @brief Find a recording device ID by its friendly name (partial match supported)
 * @param name Part of the device name (e.g. "VB-Cable")
 * @return Device ID string, or empty if not found
 */
std::string FindRecordingDeviceID(const std::string& name);

/**
 * @brief Enable or Disable a device driver by name (Requires Admin)
 * @param name Part of the device friendly name (e.g. "VB-Cable")
 * @param enable true to enable, false to disable
 * @return true if successful
 */
bool ChangeDeviceState(const std::string& name, bool enable);

/**
 * @brief Check if process has Admin privileges
 * @return true if running as Admin
 */
bool IsRunningAsAdmin();

} // namespace windows
} // namespace platform
} // namespace moonmic
