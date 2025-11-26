/**
 * @file moonmic_internal.h
 * @brief Internal types and definitions for libmoonmic
 */

#pragma once

#include "moonmic.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct moonmic_opus_encoder_t moonmic_opus_encoder_t;
typedef struct udp_sender_t udp_sender_t;
typedef struct audio_capture_t audio_capture_t;

/**
 * @brief Internal client structure
 */
struct moonmic_client_t {
    // Configuration
    moonmic_config_t config;
    
    // Components
    audio_capture_t* capture;
    moonmic_opus_encoder_t* encoder;
    udp_sender_t* sender;
    
    // State
    bool active;
    bool running;
    
    // Callbacks
    moonmic_error_callback_t error_callback;
    void* error_userdata;
    moonmic_status_callback_t status_callback;
    void* status_userdata;
    
    // Threading (platform-specific)
    void* thread_handle;
};

/**
 * @brief Audio capture interface (platform-specific)
 */
struct audio_capture_t {
    /**
     * @brief Initialize audio capture
     * @param sample_rate Sample rate in Hz
     * @param channels Number of channels (1 or 2)
     * @return true on success, false on failure
     */
    bool (*init)(audio_capture_t* self, uint32_t sample_rate, uint8_t channels);
    
    /**
     * @brief Read audio samples
     * @param buffer Output buffer (float32 format)
     * @param frames Number of frames to read
     * @return Number of frames read, or -1 on error
     */
    int (*read)(audio_capture_t* self, float* buffer, size_t frames);
    
    /**
     * @brief Close audio capture
     */
    void (*close)(audio_capture_t* self);
    
    // Platform-specific data
    void* platform_data;
};

/**
 * @brief Opus encoder wrapper
 */
struct moonmic_opus_encoder_t {
    void* encoder;  // OpusEncoder*
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t bitrate;
};

/**
 * @brief UDP sender
 */
struct udp_sender_t {
    int socket_fd;
    char host_ip[64];
    uint16_t port;
    uint32_t sequence;
};

/**
 * @brief Packet header for UDP transmission
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;      // 0x4D4D4943 ("MMIC")
    uint32_t sequence;   // Packet sequence number
    uint64_t timestamp;  // Microseconds since start
    uint32_t sample_rate; // Sample rate of encoded audio (e.g., 16000, 48000)
} moonmic_packet_header_t;

#define MOONMIC_MAGIC 0x4D4D4943
#define MOONMIC_VERSION "1.0.0"
// Header size: magic(4) + sequence(4) + timestamp(8) + sample_rate(4) = 20 bytes
// Use this constant instead of sizeof() due to compiler alignment issues on ARM
#define MOONMIC_HEADER_SIZE 20

// Platform-specific factory functions
#ifdef __vita__
audio_capture_t* audio_capture_create_vita(void);
#elif _WIN32
audio_capture_t* audio_capture_create_windows(void);
#elif __linux__
audio_capture_t* audio_capture_create_linux(void);
#elif __APPLE__
audio_capture_t* audio_capture_create_macos(void);
#elif __ANDROID__
audio_capture_t* audio_capture_create_android(void);
#endif

// Codec functions (renamed to avoid conflicts with libopus)
moonmic_opus_encoder_t* moonmic_opus_encoder_create(uint32_t sample_rate, uint8_t channels, uint32_t bitrate);
void moonmic_opus_encoder_destroy(moonmic_opus_encoder_t* encoder);
int moonmic_opus_encoder_encode(moonmic_opus_encoder_t* encoder, const float* pcm, int frame_size, 
                       uint8_t* output, int max_output_bytes);

// Speex resampler functions
typedef struct moonmic_speex_resampler_t moonmic_speex_resampler_t;
moonmic_speex_resampler_t* moonmic_speex_resampler_create(uint32_t in_rate, uint32_t out_rate, uint8_t channels);
void moonmic_speex_resampler_destroy(moonmic_speex_resampler_t* resampler);
int moonmic_speex_resampler_process(moonmic_speex_resampler_t* resampler, 
                                     const int16_t* input, uint32_t in_frames,
                                     int16_t* output, uint32_t* out_frames);


// Network functions
udp_sender_t* udp_sender_create(const char* host_ip, uint16_t port);
void udp_sender_destroy(udp_sender_t* sender);
bool udp_sender_send(udp_sender_t* sender, const void* data, size_t size);

// Utility functions
uint64_t moonmic_get_timestamp_us(void);
void* moonmic_thread_create(void* (*func)(void*), void* arg);
void moonmic_thread_join(void* thread_handle);

#ifdef __cplusplus
}
#endif
