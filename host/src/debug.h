/**
 * @file debug.h
 * @brief Debug utilities for moonmic-host
 */

#pragma once

extern bool g_debug_mode;

// Helper to determine if debug logging should be shown
inline bool isDebugMode() {
    return g_debug_mode;
}
