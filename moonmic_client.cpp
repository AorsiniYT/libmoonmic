/**
 * @file moonmic_client.cpp
 * @brief Main implementation of MoonMic client
 */

#include "moonmic.h"
#include "moonmic_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    
    const int frame_size = 480; // 10ms at 48kHz
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
    
    while (client->running) {
        // Capture audio
        int frames_read = client->capture->read(client->capture, pcm_buffer, frame_size);
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
        
        // Encode with Opus
        int encoded_bytes = moonmic_opus_encoder_encode(
            client->encoder,
            pcm_buffer,
            frames_read,
            opus_buffer + sizeof(moonmic_packet_header_t),
            4000 - sizeof(moonmic_packet_header_t)
        );
        
        if (encoded_bytes < 0) {
            if (client->error_callback) {
                client->error_callback("Opus encoding failed", client->error_userdata);
            }
            continue;
        }
        
        // Prepare packet header
        moonmic_packet_header_t* header = (moonmic_packet_header_t*)opus_buffer;
        header->magic = MOONMIC_MAGIC;
        header->sequence = client->sender->sequence++;
        header->timestamp = moonmic_get_timestamp_us();
        
        // Send via UDP
        size_t total_size = sizeof(moonmic_packet_header_t) + encoded_bytes;
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
        return NULL;
    }
    
    moonmic_client_t* client = (moonmic_client_t*)calloc(1, sizeof(moonmic_client_t));
    if (!client) {
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
    
    // Create platform-specific audio capture
#ifdef __vita__
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
        free(client);
        return NULL;
    }
    
    // Initialize audio capture
    if (!client->capture->init(client->capture, client->config.sample_rate, client->config.channels)) {
        client->capture->close(client->capture);
        free(client->capture);
        free(client);
        return NULL;
    }
    
    // Create Opus encoder
    client->encoder = moonmic_opus_encoder_create(
        client->config.sample_rate,
        client->config.channels,
        client->config.bitrate
    );
    if (!client->encoder) {
        client->capture->close(client->capture);
        free(client->capture);
        free(client);
        return NULL;
    }
    
    // Create UDP sender
    client->sender = udp_sender_create(client->config.host_ip, client->config.port);
    if (!client->sender) {
        moonmic_opus_encoder_destroy(client->encoder);
        client->capture->close(client->capture);
        free(client->capture);
        free(client);
        return NULL;
    }
    
    // Auto-start if requested
    if (client->config.auto_start) {
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
