/**
 * @file moonmic_client.cpp
 * @brief Main implementation of MoonMic client
 */

#include "moonmic.h"
#include "moonmic_internal.h"
#include "moonmic_debug.h"
#include "heartbeat_monitor.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

// Include platform-specific configuration
#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include "platform/psvita/platform_config.h"
#elif defined(_WIN32)
#include "platform/windows/platform_config.h"
#else
#include "platform/linux/platform_config.h"
#endif

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
    
    // Send handshake packet first (and re-send every 3 seconds if not validated)
    moonmic_handshake_t handshake = {0};
    handshake.magic = 0x4D4F4F4E;  // "MOON"
    handshake.version = 2;  // Bumped for protocol extension
    handshake.pair_status = client->config.pair_status;
    
    // Display resolution (0 = don't configure, non-zero = configure)
    handshake.display_width = client->config.target_display_width;
    handshake.display_height = client->config.target_display_height;
    
    // Copy uniqueid and devicename from config if provided
    if (client->config.uniqueid && client->config.uniqueid[0]) {
        handshake.uniqueid_len = (uint8_t)strlen(client->config.uniqueid);
        if (handshake.uniqueid_len > 16) handshake.uniqueid_len = 16;
        memcpy(handshake.uniqueid, client->config.uniqueid, handshake.uniqueid_len);
    }
    
    if (client->config.devicename && client->config.devicename[0]) {
        handshake.devicename_len = (uint8_t)strlen(client->config.devicename);
        if (handshake.devicename_len > 64) handshake.devicename_len = 64;
        memcpy(handshake.devicename, client->config.devicename, handshake.devicename_len);
    }
    
    // Send initial handshake
    if (udp_sender_send(client->sender, &handshake, sizeof(handshake))) {
        MOONMIC_LOG("[moonmic_worker] Handshake sent: device='%s', uniqueid_len=%d, resolution=%dx%d", 
                   client->config.devicename ? client->config.devicename : "unknown",
                   handshake.uniqueid_len,
                   handshake.display_width, handshake.display_height);
    } else {
        MOONMIC_LOG("[moonmic_worker] WARNING: Failed to send handshake");
    }
    
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
    bool was_connected = true;  // Track previous connection state for handshake re-send
    uint64_t last_probe_time = 0;  // For periodic probing when disconnected
    static int probe_count = 0;   // Track probe attempts
    
    // Vita-optimized probe interval: 3 seconds (balanced between responsiveness and battery)
    const uint64_t PROBE_INTERVAL_MS = 3000;
    
    // FLUSH BUFFER: Read and discard potential stale audio accumulated during connection setup
    // Read up to 10 frames or until empty
    float* dump_buffer = (float*)malloc(frame_size * client->config.channels * sizeof(float));
    if (dump_buffer) {
        MOONMIC_LOG("[moonmic_worker] Flushing audio buffer...");
        for (int i = 0; i < 10; i++) {
            // Read non-blocking if possible, but Vita API is blocking. 
            // We assume max buffer isn't huge. Just read a few frames.
            int read = client->capture->read(client->capture, dump_buffer, frame_size);
            if (read <= 0) break;
        }
        free(dump_buffer);
        MOONMIC_LOG("[moonmic_worker] Flush complete.");
    }
    
    while (client->running) {
        // Check heartbeat status - if host disconnected, wait and resend handshake when reconnected
        if (client->heartbeat_monitor) {
            bool is_connected = heartbeat_monitor_is_connected(client->heartbeat_monitor);
            
            if (!is_connected && was_connected) {
                // Just disconnected - enter suspension mode
                MOONMIC_LOG("[moonmic_worker] Host disconnected - entering suspension mode");
                MOONMIC_LOG("[moonmic_worker] Probing for host every 3 seconds...");
                was_connected = false;
                last_probe_time = 0;  // Force immediate probe
                probe_count = 0;
            }
            
            if (!is_connected) {
                // In suspension mode - periodically send handshake probe to detect host
                uint64_t now = moonmic_get_timestamp_us() / 1000;  // Convert to ms
                
                if (now - last_probe_time >= PROBE_INTERVAL_MS) {
                    probe_count++;
                    // Send handshake as a probe to detect if host is available
                    udp_sender_send(client->sender, &handshake, sizeof(handshake));
                    MOONMIC_LOG("[moonmic_worker] Probe #%d: waiting for host...", probe_count);
                    last_probe_time = now;
                }
                
                // Sleep 200ms - good balance for Vita (not too aggressive, reasonably responsive)
#ifdef _WIN32
                Sleep(200);
#else
                usleep(200000);  // 200ms
#endif
                continue;
            }
            
            if (is_connected && !was_connected) {
                // Just reconnected - host is back online!
                MOONMIC_LOG("[moonmic_worker] Host is back online! Resuming transmission...");
                if (udp_sender_send(client->sender, &handshake, sizeof(handshake))) {
                    MOONMIC_LOG("[moonmic_worker] Handshake sent - resuming audio");
                }
                was_connected = true;
                probe_count = 0;
            }
            
            // Check if host has paused transmission (STOP signal received)
            if (is_connected && heartbeat_monitor_is_paused(client->heartbeat_monitor)) {
                // Host is connected but has sent STOP signal - pause audio transmission
#ifdef _WIN32
                Sleep(100);  // Sleep 100ms
#else
                usleep(100000);  // Sleep 100ms
#endif
                continue;  // Skip audio capture and transmission
            }
        }
        
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
        
        // RAW mode: send immediately without accumulation
        if (client->config.raw_mode) {
            // Convert float to int16 for transmission
            int16_t* pcm_int16 = (int16_t*)(opus_buffer + MOONMIC_HEADER_SIZE);
            for (int i = 0; i < frames_read * client->config.channels; i++) {
                float sample = pcm_buffer[i];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                pcm_int16[i] = (int16_t)(sample * 32767.0f);
            }
            int encoded_bytes = frames_read * client->config.channels * sizeof(int16_t);
            uint32_t packet_sample_rate = client->config.sample_rate | MOONMIC_RAW_FLAG;
            
            // Prepare and send packet (same header logic as before)
            uint8_t* header_ptr = opus_buffer;
            uint32_t magic = MOONMIC_MAGIC;
            header_ptr[0] = (magic >> 0) & 0xFF;
            header_ptr[1] = (magic >> 8) & 0xFF;
            header_ptr[2] = (magic >> 16) & 0xFF;
            header_ptr[3] = (magic >> 24) & 0xFF;
            
            uint32_t seq = client->sender->sequence++;
            header_ptr[4] = (seq >> 0) & 0xFF;
            header_ptr[5] = (seq >> 8) & 0xFF;
            header_ptr[6] = (seq >> 16) & 0xFF;
            header_ptr[7] = (seq >> 24) & 0xFF;
            
            uint64_t ts = moonmic_get_timestamp_us();
            header_ptr[8] = (ts >> 0) & 0xFF;
            header_ptr[9] = (ts >> 8) & 0xFF;
            header_ptr[10] = (ts >> 16) & 0xFF;
            header_ptr[11] = (ts >> 24) & 0xFF;
            header_ptr[12] = (ts >> 32) & 0xFF;
            header_ptr[13] = (ts >> 40) & 0xFF;
            header_ptr[14] = (ts >> 48) & 0xFF;
            header_ptr[15] = (ts >> 56) & 0xFF;
            
            header_ptr[16] = (packet_sample_rate >> 0) & 0xFF;
            header_ptr[17] = (packet_sample_rate >> 8) & 0xFF;
            header_ptr[18] = (packet_sample_rate >> 16) & 0xFF;
            header_ptr[19] = (packet_sample_rate >> 24) & 0xFF;
            
            size_t total_size = MOONMIC_HEADER_SIZE + encoded_bytes;
            udp_sender_send(client->sender, opus_buffer, total_size);
            continue;  // Skip Opus encoding
        }
        
        // OPUS MODE: Accumulate frames until we reach 320 samples (20ms @ 16kHz)
        int samples_to_copy = frames_read * client->config.channels;
        int space_available = (client->target_frame_size - client->accumulated_samples) * client->config.channels;
        
        
        // Apply gain to samples (BEFORE encoding)
        // This is critical because Vita microphone has very low volume
        // Gain from config (default: 10.0x, adjustable 1.0-100.0 in GUI)
        const float GAIN = client->config.gain;
        
        for (int i = 0; i < samples_to_copy; i++) {
            pcm_buffer[i] *= GAIN;
            
            // Clamp to prevent overflow (hard clipping)
            if (pcm_buffer[i] > 1.0f) pcm_buffer[i] = 1.0f;
            if (pcm_buffer[i] < -1.0f) pcm_buffer[i] = -1.0f;
        }
        
        // DEBUG: Log accumulation state
        static int accum_log_count = 0;
        if (accum_log_count < 10) {
            MOONMIC_LOG("[ACCUM] Read %d frames (%d samples), buffer has %zu/%zu",
                       frames_read, samples_to_copy, client->accumulated_samples, client->target_frame_size);
            MOONMIC_LOG("[ACCUM] First 5 AMPLIFIED samples (gain=%.1fx): %.4f %.4f %.4f %.4f %.4f",
                       GAIN, pcm_buffer[0], pcm_buffer[1], pcm_buffer[2], pcm_buffer[3], pcm_buffer[4]);
            accum_log_count++;
        }
        
        if (samples_to_copy <= space_available) {
            // Copy all samples to accumulation buffer
            memcpy(client->accumulation_buffer + client->accumulated_samples * client->config.channels,
                   pcm_buffer,
                   samples_to_copy * sizeof(float));
            client->accumulated_samples += frames_read;
            
            if (accum_log_count < 10) {
                MOONMIC_LOG("[ACCUM] Copied ALL %d frames, new total: %zu/%zu",
                           frames_read, client->accumulated_samples, client->target_frame_size);
            }
        } else {
            // Copy only what fits to complete the current frame
            int frames_to_copy = space_available; // Single channel logic for simplicity in variable name, but handles channels below
            
            memcpy(client->accumulation_buffer + client->accumulated_samples * client->config.channels,
                   pcm_buffer,
                   frames_to_copy * client->config.channels * sizeof(float));
            client->accumulated_samples += frames_to_copy;
            
            // At this point buffer is FULL (320 samples) -> Encode and Send happens below
            // WE MUST REMEMBER TO COPY THE REST after sending!
            // We'll handle this by checking if we have leftovers AFTER the send block
        }
        
        // Calculate leftovers for later
        int samples_leftover = 0;
        int leftover_offset = 0;
        if (samples_to_copy > space_available) {
             samples_leftover = samples_to_copy - space_available;
             leftover_offset = space_available;
        }
        
        // When we have enough samples (320), encode with Opus
        if (client->accumulated_samples >= client->target_frame_size) {
            MOONMIC_LOG("[OPUS_ENCODE] Encoding %zu samples from accumulation buffer", client->accumulated_samples);
            MOONMIC_LOG("[OPUS_ENCODE] First 10 buffer samples: %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f",
                       client->accumulation_buffer[0], client->accumulation_buffer[1],
                       client->accumulation_buffer[2], client->accumulation_buffer[3],
                       client->accumulation_buffer[4], client->accumulation_buffer[5],
                       client->accumulation_buffer[6], client->accumulation_buffer[7],
                       client->accumulation_buffer[8], client->accumulation_buffer[9]);
            
            int encoded_bytes = moonmic_opus_encoder_encode(
                client->encoder,
                client->accumulation_buffer,
                client->target_frame_size,
                opus_buffer + MOONMIC_HEADER_SIZE,
                4000 - MOONMIC_HEADER_SIZE
            );
            
            MOONMIC_LOG("[OPUS_ENCODE] Encoded result: %d bytes", encoded_bytes);
            
            if (encoded_bytes < 0) {
                if (client->error_callback) {
                    client->error_callback("Opus encoding failed", client->error_userdata);
                }
                client->accumulated_samples = 0;  // Reset on error
                MOONMIC_LOG("[OPUS_ENCODE] ERROR: Encoding failed, resetting buffer");
                continue;
            }
            
            uint32_t packet_sample_rate = client->config.sample_rate;  // No RAW flag
            
            // Prepare packet header
            uint8_t* header_ptr = opus_buffer;
            uint32_t magic = MOONMIC_MAGIC;
            header_ptr[0] = (magic >> 0) & 0xFF;
            header_ptr[1] = (magic >> 8) & 0xFF;
            header_ptr[2] = (magic >> 16) & 0xFF;
            header_ptr[3] = (magic >> 24) & 0xFF;
            
            uint32_t seq = client->sender->sequence++;
            header_ptr[4] = (seq >> 0) & 0xFF;
            header_ptr[5] = (seq >> 8) & 0xFF;
            header_ptr[6] = (seq >> 16) & 0xFF;
            header_ptr[7] = (seq >> 24) & 0xFF;
            
            uint64_t ts = moonmic_get_timestamp_us();
            header_ptr[8] = (ts >> 0) & 0xFF;
            header_ptr[9] = (ts >> 8) & 0xFF;
            header_ptr[10] = (ts >> 16) & 0xFF;
            header_ptr[11] = (ts >> 24) & 0xFF;
            header_ptr[12] = (ts >> 32) & 0xFF;
            header_ptr[13] = (ts >> 40) & 0xFF;
            header_ptr[14] = (ts >> 48) & 0xFF;
            header_ptr[15] = (ts >> 56) & 0xFF;
            
            header_ptr[16] = (packet_sample_rate >> 0) & 0xFF;
            header_ptr[17] = (packet_sample_rate >> 8) & 0xFF;
            header_ptr[18] = (packet_sample_rate >> 16) & 0xFF;
            header_ptr[19] = (packet_sample_rate >> 24) & 0xFF;
            
            // Send via UDP
            size_t total_size = MOONMIC_HEADER_SIZE + encoded_bytes;
            udp_sender_send(client->sender, opus_buffer, total_size);
            
            // Reset accumulation buffer for next frame
            client->accumulated_samples = 0;
            
            // If we had leftovers from the input buffer, copy them now
            if (samples_leftover > 0) {
               memcpy(client->accumulation_buffer, 
                      pcm_buffer + leftover_offset, // pcm_buffer is float* so pointer arithmetic works on samples
                      samples_leftover * sizeof(float));
               client->accumulated_samples = samples_leftover / client->config.channels; // Assuming leftovers are multiple of channels
               
               if (accum_log_count < 10) {
                   MOONMIC_LOG("[ACCUM] Carried over %d leftover samples to new frame", samples_leftover);
               }
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
    
    // Copy strings to internal storage to prevent dangling pointers
    if (config->uniqueid && config->uniqueid[0]) {
        strncpy(client->uniqueid_storage, config->uniqueid, sizeof(client->uniqueid_storage) - 1);
        client->uniqueid_storage[sizeof(client->uniqueid_storage) - 1] = '\0';
        client->config.uniqueid = client->uniqueid_storage;
    } else {
        client->uniqueid_storage[0] = '\0';
        client->config.uniqueid = NULL;
    }
    
    if (config->devicename && config->devicename[0]) {
        strncpy(client->devicename_storage, config->devicename, sizeof(client->devicename_storage) - 1);
        client->devicename_storage[sizeof(client->devicename_storage) - 1] = '\0';
        client->config.devicename = client->devicename_storage;
    } else {
        client->devicename_storage[0] = '\0';
        client->config.devicename = NULL;
    }
    
    MOONMIC_LOG("[moonmic_create] Copied strings: uniqueid='%s', devicename='%s'", 
                client->config.uniqueid ? client->config.uniqueid : "(null)",
                client->config.devicename ? client->config.devicename : "(null)");
    
    // Initialize state
    client->handshake_sent = false;

    
    // Initialize accumulation buffer (for Opus frame batching)
    client->accumulation_buffer = NULL;
    client->accumulated_samples = 0;
    client->target_frame_size = 320;  // 20ms @ 16kHz (valid Opus frame size)
    
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
    
    
    // Create Opus encoder (skip in RAW mode)
    if (client->config.raw_mode) {
        MOONMIC_LOG("[moonmic_create] RAW mode enabled - skipping Opus encoder");
        client->encoder = NULL;
    } else {
        MOONMIC_LOG("[moonmic_create] Creating Opus encoder");
        
        // Get native sample rate from platform (e.g., 16kHz for Vita)
        uint32_t encoder_sample_rate = client->capture->get_native_sample_rate(client->capture);
        uint32_t encoder_bitrate = client->config.bitrate;
        
        MOONMIC_LOG("[moonmic_create] Using %uHz for Opus (platform native rate)", encoder_sample_rate);
        
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
    }
    
    MOONMIC_LOG("[moonmic_create] Creating UDP sender to %s:%d", client->config.host_ip, client->config.port);
    // Create UDP sender
    client->sender = udp_sender_create(client->config.host_ip, client->config.port);
    if (!client->sender) {
        MOONMIC_LOG("[moonmic_create] ERROR: Failed to create UDP sender");
        moonmic_destroy(client);
        return NULL;
    }
    
    // Create heartbeat monitor
    // IMPORTANT: We must use the SAME socket as the sender to receive ACKs/PINGs
    // The host replies to the source port of our audio packets.
    if (client->sender) {
        // Pass IP/Port for active RTT pinging
        client->heartbeat_monitor = heartbeat_monitor_create(
            client->sender->socket_fd, 
            client->config.host_ip, 
            client->config.port
        );
        
        if (client->heartbeat_monitor) {
            MOONMIC_LOG("[moonmic_create] Heartbeat monitor started on shared socket %d", 
                        client->sender->socket_fd);
        } else {
            MOONMIC_LOG("[moonmic_create] Failed to create heartbeat monitor");
        }
    }
    
    // Allocate accumulation buffer for Opus mode (for 320-sample batching)
    if (!client->config.raw_mode) {
        size_t buffer_size = client->target_frame_size * client->config.channels;
        client->accumulation_buffer = (float*)malloc(buffer_size * sizeof(float));
        if (!client->accumulation_buffer) {
            MOONMIC_LOG("[moonmic_create] ERROR: Failed to allocate accumulation buffer");
            udp_sender_destroy(client->sender);
            moonmic_opus_encoder_destroy(client->encoder);
            client->capture->close(client->capture);
            free(client->capture);
            free(client);
            return NULL;
        }
        memset(client->accumulation_buffer, 0, buffer_size * sizeof(float));
        MOONMIC_LOG("[moonmic_create] Allocated accumulation buffer: %zu samples", buffer_size);
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
    
    if (client->accumulation_buffer) {
        free(client->accumulation_buffer);
    }
    if (client->heartbeat_monitor) {
        heartbeat_monitor_destroy(client->heartbeat_monitor);
    }
    
    free(client);
    MOONMIC_LOG("[moonmic_destroy] Client destroyed");
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

void moonmic_set_gain(moonmic_client_t* client, float gain) {
    if (client) {
        client->config.gain = gain;
    }
}

moonmic_connection_status_t moonmic_get_connection_status(moonmic_client_t* client) {
    if (!client || !client->heartbeat_monitor) {
        return MOONMIC_DISCONNECTED;
    }
    return heartbeat_monitor_get_status(client->heartbeat_monitor);
}

bool moonmic_is_paused(moonmic_client_t* client) {
    if (!client || !client->heartbeat_monitor) return false;
    return heartbeat_monitor_is_paused(client->heartbeat_monitor);
}

int moonmic_client_get_rtt(moonmic_client_t* client) {
    if (!client || !client->heartbeat_monitor) return -1;
    return heartbeat_monitor_get_rtt(client->heartbeat_monitor);
}

bool moonmic_is_connected(moonmic_client_t* client) {
    return moonmic_get_connection_status(client) == MOONMIC_CONNECTED;
}
