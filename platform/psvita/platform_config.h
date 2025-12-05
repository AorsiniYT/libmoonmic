/**
 * @file platform_config.h
 * @brief PS Vita platform-specific microphone configuration
 */

#pragma once

// PS Vita microphone hardware constraints
#define PLATFORM_SAMPLE_RATE 16000       // Vita mic max is 16kHz (48kHz not supported)
#define PLATFORM_CHANNELS 1              // Mono only
#define PLATFORM_GRAIN_SIZE 256          // 256 samples for 16kHz (16ms)
#define PLATFORM_BITRATE 96000           // 96kbps for better voice quality (was 64kbps)

// Buffer sizes
#define PLATFORM_PCM_BUFFER_SIZE 4096    // PCM buffer size in frames
#define PLATFORM_OPUS_BUFFER_SIZE 4000   // Opus packet buffer size in bytes

// Vita needs padding from 256 to 320 for Opus frame size compatibility (20ms @ 16kHz)
#define PLATFORM_NEEDS_PADDING 1
#define PLATFORM_PADDED_FRAME_SIZE 320   // 20ms @ 16kHz (standard Opus frame)

// Platform name for logging
#define PLATFORM_NAME "PS Vita"
