/**
 * @file moonmic_client.cpp
 * @brief Main implementation of MoonMic client
 */

#include "moonmic.h"
#include "moonmic_internal.h"
#include "moonmic_debug.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#endif

// Worker thread function
static void* moonmic_worker_thread(void* arg) {
    moonmic_client_t* client = (moonmic_client_t*)arg;
    
    // Vita: 256 samples @ 16kHz → 320 samples for Opus (padded)
    // Other platforms: 480 samples @ 48kHz
    const int frame_size = 480; // Will be adjusted per-platform
    const int buffer_size = frame_size * client->config.channels;
    float* pcm_buffer = (float*)malloc(buffer_size * sizeof(float));
    uint8_t* opus_buffer = (uint8_t*)malloc(4000); // Max Opus packet size
    
    if (!pcm_buffer || !opus_buffer) {
        if (client->error_callback) {
            client->error_callback("Failed to allocate buffers", client->error_userdata);
        }
        free(pcm_buffer);
        free(opus_buffer);
        return NULL;
    }
    
    MOONMIC_LOG("[moonmic_worker] Thread started - beginning capture loop");
    int loop_count = 0;
    
    while (client->running) {
        // Capture audio
        int frames_read = client->capture->read(client->capture, pcm_buffer, frame_size);
        
        // DEBUG: Log first few iterations
        if (loop_count < 3) {
            MOONMIC_LOG("[moonmic_worker] Loop %d: frames_read = %d", loop_count, frames_read);
            loop_count++;
        }

        if (frames_read < 0) {
            if (client->error_callback) {
                client->error_callback("Audio capture failed", client->error_userdata);
            }
            break;
        }
        
        if (frames_read == 0) {
            // No data available, sleep briefly
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1000);
#endif
            continue;
        }
        
        // Vita captures 256 samples @ 16kHz, pad to 320 for Opus
        int opus_frame_size = frames_read;
#ifdef __vita__
        if (frames_read == 256) {
            // Pad with last sample to reach 320 (valid Opus frame size @ 16kHz)
            for (int i = 256; i < 320; i++) {
                pcm_buffer[i] = pcm_buffer[255];
            }
            opus_frame_size = 320;
        }
#endif
        
        // Encode with Opus
        int encoded_bytes = moonmic_opus_encoder_encode(
            client->encoder,
            pcm_buffer,
            opus_frame_size,
            opus_buffer + MOONMIC_HEADER_SIZE,
            4000 - MOONMIC_HEADER_SIZE
        );
        
        if (encoded_bytes < 0) {
            if (client->error_callback) {
                client->error_callback("Opus encoding failed", client->error_userdata);
            }
            continue;
        }
        
        // Prepare packet header - WRITE MANUALLY to ensure correct byte order
        // We write manually because ARM compiler struct packing is unreliable
        uint8_t* header_ptr = opus_buffer;
        
        // Magic (0x4D4D4943 = "MMIC") - 4 bytes, little-endian
        uint32_t magic = MOONMIC_MAGIC;
        header_ptr[0] = (magic >> 0) & 0xFF;
        header_ptr[1] = (magic >> 8) & 0xFF;
        header_ptr[2] = (magic >> 16) & 0xFF;
        header_ptr[3] = (magic >> 24) & 0xFF;
        
        // Sequence - 4 bytes, little-endian
        uint32_t seq = client->sender->sequence++;
        header_ptr[4] = (seq >> 0) & 0xFF;
        header_ptr[5] = (seq >> 8) & 0xFF;
        header_ptr[6] = (seq >> 16) & 0xFF;
        header_ptr[7] = (seq >> 24) & 0xFF;
        
        // Timestamp - 8 bytes, little-endian
        uint64_t ts = moonmic_get_timestamp_us();
        header_ptr[8] = (ts >> 0) & 0xFF;
        header_ptr[9] = (ts >> 8) & 0xFF;
        header_ptr[10] = (ts >> 16) & 0xFF;
        header_ptr[11] = (ts >> 24) & 0xFF;
        header_ptr[12] = (ts >> 32) & 0xFF;
        header_ptr[13] = (ts >> 40) & 0xFF;
        header_ptr[14] = (ts >> 48) & 0xFF;
        header_ptr[15] = (ts >> 56) & 0xFF;
        
        // Sample rate - 4 bytes, little-endian
        uint32_t sr = client->encoder->sample_rate;
        header_ptr[16] = (sr >> 0) & 0xFF;
        header_ptr[17] = (sr >> 8) & 0xFF;
        header_ptr[18] = (sr >> 16) & 0xFF;
        header_ptr[19] = (sr >> 24) & 0xFF;
        
        // DEBUG: Log first packet
        if (seq == 0) {  // First packet (before increment)
            MOONMIC_LOG("[moonmic_worker] FIRST PACKET (manual write):");
            MOONMIC_LOG("  magic = 0x%08X", magic);
            MOONMIC_LOG("  sequence = %u", seq);
            MOONMIC_LOG("  timestamp = %llu", (unsigned long long)ts);
            MOONMIC_LOG("  sample_rate = %u", sr);
            MOONMIC_LOG("  Raw bytes:");
            for (int i = 0; i < 20; i++) {
                MOONMIC_LOG("    [%02d] = 0x%02X", i, header_ptr[i]);
            }
        }
        
        // Send via UDP
        size_t total_size = MOONMIC_HEADER_SIZE + encoded_bytes;
        if (!udp_sender_send(client->sender, opus_buffer, total_size)) {
            if (client->error_callback) {
                client->error_callback("UDP send failed", client->error_userdata);
            }
        }
    }
    
    free(pcm_buffer);
    free(opus_buffer);
    return NULL;
}

moonmic_client_t* moonmic_create(const moonmic_config_t* config) {
    if (!config || !config->host_ip) {
        MOONMIC_LOG("[moonmic_create] ERROR: Invalid config or host_ip is NULL");
        return NULL;
    }
    
    MOONMIC_LOG("[moonmic_create] Creating client for %s:%d", config->host_ip, config->port);
    
    moonmic_client_t* client = (moonmic_client_t*)calloc(1, sizeof(moonmic_client_t));
    if (!client) {
        MOONMIC_LOG("[moonmic_create] ERROR: Failed to allocate client memory");
        return NULL;
    }
    
    // Copy configuration
    client->config = *config;
    
    // Set defaults
    if (client->config.port == 0) {
        client->config.port = 48100;
    }
    if (client->config.sample_rate == 0) {
        client->config.sample_rate = 48000;
    }
    if (client->config.channels == 0) {
        client->config.channels = 1;
    }
    if (client->config.bitrate == 0) {
        client->config.bitrate = 64000;
    }
    
    MOONMIC_LOG("[moonmic_create] Config: %dHz, %dch, %dbps, port=%d",
        client->config.sample_rate, client->config.channels, client->config.bitrate, client->config.port);
    
    // Create platform-specific audio capture
#ifdef __vita__
    MOONMIC_LOG("[moonmic_create] Creating Vita audio capture");
    client->capture = audio_capture_create_vita();
#elif _WIN32
    client->capture = audio_capture_create_windows();
#elif __linux__
    client->capture = audio_capture_create_linux();
#elif __APPLE__
    client->capture = audio_capture_create_macos();
#elif __ANDROID__
    client->capture = audio_capture_create_android();
#else
    #error "Unsupported platform"
#endif
    
    if (!client->capture) {
        MOONMIC_LOG("[moonmic_create] ERROR: Failed to create audio capture");
        free(client);
        return NULL;
    }
    
    MOONMIC_LOG("[moonmic_create] Initializing audio capture");
    // Initialize audio capture
    if (!client->capture->init(client->capture, client->config.sample_rate, client->config.channels)) {
        MOONMIC_LOG("[moonmic_create] ERROR: Failed to initialize audio capture");
        client->capture->close(client->capture);
        free(client->capture);
        free(client);
        return NULL;
    }
    
    MOONMIC_LOG("[moonmic_create] Creating Opus encoder");
    
    // Use 16kHz for Vita (matching hardware), 48kHz for other platforms
   uint32_t encoder_sample_rate = client->config.sample_rate;
    uint32_t encoder_bitrate = client->config.bitrate;
    
#ifdef __vita__
    // Vita captures at 16kHz (hardware), encode at same rate
    // Host will resample 16kHz→48kHz for output
    encoder_sample_rate = 16000;
    encoder_bitrate = 24000;  // 24kbps is optimal for 16kHz mono VOIP
    MOONMIC_LOG("[moonmic_create] Using 16kHz for Opus (matching Vita mic hardware)");
#endif
    
    // Create Opus encoder
    client->encoder = moonmic_opus_encoder_create(
        encoder_sample_rate,
        client->config.channels,
        encoder_bitrate
    );
    if (!client->encoder) {
        MOONMIC_LOG("[moonmic_create] ERROR: Failed to create Opus encoder");
        client->capture->close(client->capture);
        free(client->capture);
        free(client);
        return NULL;
    }
    
    MOONMIC_LOG("[moonmic_create] Creating UDP sender to %s:%d", client->config.host_ip, client->config.port);
    // Create UDP sender
    client->sender = udp_sender_create(client->config.host_ip, client->config.port);
    if (!client->sender) {
        MOONMIC_LOG("[moonmic_create] ERROR: Failed to create UDP sender");
        moonmic_opus_encoder_destroy(client->encoder);
        client->capture->close(client->capture);
        free(client->capture);
        free(client);
        return NULL;
    }
    
    MOONMIC_LOG("[moonmic_create] Client created successfully");
    
    // Auto-start if requested
    if (client->config.auto_start) {
        MOONMIC_LOG("[moonmic_create] Auto-starting client");
        moonmic_start(client);
    }
    
    return client;
}

void moonmic_destroy(moonmic_client_t* client) {
    if (!client) {
        return;
    }
    
    moonmic_stop(client);
    
    if (client->sender) {
        udp_sender_destroy(client->sender);
    }
    if (client->encoder) {
        moonmic_opus_encoder_destroy(client->encoder);
    }
    if (client->capture) {
        client->capture->close(client->capture);
        free(client->capture);
    }
    
    free(client);
}

bool moonmic_start(moonmic_client_t* client) {
    if (!client || client->active) {
        return false;
    }
    
    client->running = true;
    client->active = true;
    
    // Create worker thread
    client->thread_handle = moonmic_thread_create(moonmic_worker_thread, client);
    if (!client->thread_handle) {
        client->running = false;
        client->active = false;
        return false;
    }
    
    if (client->status_callback) {
        client->status_callback(true, client->status_userdata);
    }
    
    return true;
}

void moonmic_stop(moonmic_client_t* client) {
    if (!client || !client->active) {
        return;
    }
    
    client->running = false;
    
    if (client->thread_handle) {
        moonmic_thread_join(client->thread_handle);
        client->thread_handle = NULL;
    }
    
    client->active = false;
    
    if (client->status_callback) {
        client->status_callback(false, client->status_userdata);
    }
}

bool moonmic_is_active(moonmic_client_t* client) {
    return client && client->active;
}

void moonmic_set_error_callback(moonmic_client_t* client, 
                                moonmic_error_callback_t callback, 
                                void* userdata) {
    if (client) {
        client->error_callback = callback;
        client->error_userdata = userdata;
    }
}

void moonmic_set_status_callback(moonmic_client_t* client,
                                 moonmic_status_callback_t callback,
                                 void* userdata) {
    if (client) {
        client->status_callback = callback;
        client->status_userdata = userdata;
    }
}

const char* moonmic_version(void) {
    return MOONMIC_VERSION;
}

// Utility functions implementation
uint64_t moonmic_get_timestamp_us(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000) / frequency.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}

void* moonmic_thread_create(void* (*func)(void*), void* arg) {
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    return (void*)thread;
#else
    pthread_t* thread = (pthread_t*)malloc(sizeof(pthread_t));
    if (!thread) {
        return NULL;
    }
    if (pthread_create(thread, NULL, func, arg) != 0) {
        free(thread);
        return NULL;
    }
    return thread;
#endif
}

void moonmic_thread_join(void* thread_handle) {
    if (!thread_handle) {
        return;
    }
#ifdef _WIN32
    WaitForSingleObject((HANDLE)thread_handle, INFINITE);
    CloseHandle((HANDLE)thread_handle);
#else
    pthread_t* thread = (pthread_t*)thread_handle;
    pthread_join(*thread, NULL);
    free(thread);
#endif
}
