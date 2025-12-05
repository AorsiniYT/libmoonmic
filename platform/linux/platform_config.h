/**
 * @file platform_config.h
 * @brief Linux platform-specific microphone configuration
 */

#pragma once

// Linux microphone configuration (PulseAudio)
#define PLATFORM_SAMPLE_RATE 48000       // Standard Linux audio
#define PLATFORM_CHANNELS 1              // Mono
#define PLATFORM_GRAIN_SIZE 480          // 10ms @ 48kHz
#define PLATFORM_BITRATE 64000           // 64kbps for good voice quality

// Buffer sizes
#define PLATFORM_PCM_BUFFER_SIZE 4096    // PCM buffer size in frames
#define PLATFORM_OPUS_BUFFER_SIZE 4000   // Opus packet buffer size in bytes

// Linux doesn't need padding
#define PLATFORM_NEEDS_PADDING 0
#define PLATFORM_PADDED_FRAME_SIZE 480   // Same as grain

// Platform name for logging
#define PLATFORM_NAME "Linux"
