/**
 * @file audio_capture_vita.cpp
 * @brief PS Vita audio capture implementation using SceAudioIn
 */

#include "moonmic_internal.h"
#include "moonmic_debug.h"
#include "platform_config.h"  // Platform-specific configuration
#include <psp2/audioin.h>
#include <psp2/kernel/threadmgr.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>  // For memalign()

typedef struct {
    int port;
    int grain;
    int channels;
    int16_t* temp_buffer;  // Vita mic: S16_MONO = SIGNED 16-bit
} vita_audio_data_t;

static bool vita_audio_init(audio_capture_t* self, uint32_t sample_rate, uint8_t channels) {
    MOONMIC_LOG("[audio_capture_vita] Init called: %uHz, %dch\n", sample_rate, channels);
    
    // Validate against platform constraints
    if (sample_rate != PLATFORM_SAMPLE_RATE) {
        MOONMIC_LOG("[audio_capture_vita] WARNING: Requested %uHz, but Vita only supports %dHz\n", 
                    sample_rate, PLATFORM_SAMPLE_RATE);
    }
    
    if (channels != PLATFORM_CHANNELS) {
        MOONMIC_LOG("[audio_capture_vita] WARNING: Requested %d channels, but Vita only supports %d\n",
                    channels, PLATFORM_CHANNELS);
    }
    
    vita_audio_data_t* data = (vita_audio_data_t*)calloc(1, sizeof(vita_audio_data_t));
    if (!data) {
        MOONMIC_LOG("[audio_capture_vita] ERROR: Failed to allocate vita_audio_data_t\n");
        return false;
    }
    
    // Use platform-specific grain size (Vita requires 256, other values fail)
    data->grain = PLATFORM_GRAIN_SIZE;
    data->channels = PLATFORM_CHANNELS;
    
    MOONMIC_LOG("[audio_capture_vita] Using %dHz, grain=%d (%s hardware)\n", 
                PLATFORM_SAMPLE_RATE, PLATFORM_GRAIN_SIZE, PLATFORM_NAME);
    MOONMIC_LOG("[audio_capture_vita] Opening audio port: TYPE=VOICE, grain=%d, freq=%d\n", 
                data->grain, PLATFORM_SAMPLE_RATE);
    
    // Use VOICE port (same as lpp-vita reference implementation)
    // Note: RAW port may need additional configuration we're missing
    data->port = sceAudioInOpenPort(
        SCE_AUDIO_IN_PORT_TYPE_VOICE, 
        data->grain,
        PLATFORM_SAMPLE_RATE,
        SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO
    );
    
    if (data->port < 0) {
        MOONMIC_LOG("[audio_capture_vita] ERROR: sceAudioInOpenPort failed: 0x%08X\n", data->port);
        free(data);
        return false;
    }
    
    MOONMIC_LOG("[audio_capture_vita] Audio port opened successfully: %d\n", data->port);
    
    // Allocate temporary buffer for S16 samples (16kHz)
    
    // CRITICAL: Vita audio hardware requires 256-byte aligned buffers
    data->temp_buffer = (int16_t*)memalign(PLATFORM_GRAIN_SIZE, 
                                           data->grain * channels * sizeof(int16_t));
    if (!data->temp_buffer) {
        MOONMIC_LOG("[audio_capture_vita] ERROR: Failed to allocate aligned temp buffer\n");
        sceAudioInReleasePort(data->port);
        free(data);
        return false;
    }
    
    // Clear buffer 
    memset(data->temp_buffer, 0, data->grain * channels * sizeof(int16_t));
    
    self->platform_data = data;
    MOONMIC_LOG("[audio_capture_vita] Init completed successfully (%dHz %s)\n", 
                PLATFORM_SAMPLE_RATE, PLATFORM_NAME);
    return true;
}

static int vita_audio_read(audio_capture_t* self, float* buffer, size_t frames) {
    vita_audio_data_t* data = (vita_audio_data_t*)self->platform_data;
    if (!data || !buffer) {
        return -1;
    }
    
    // Limit frames to grain size (256 samples @ 16kHz)
    if (frames > (size_t)data->grain) {
        frames = data->grain;
    }
    
    // Read S16 samples from microphone (16kHz, 256 samples)
    int result = sceAudioInInput(data->port, data->temp_buffer);
    if (result < 0) {
        return -1;
    }
    
    // DEBUG: Log first few raw samples (only once)
    static bool logged_vita_samples = false;
    if (!logged_vita_samples) {
        logged_vita_samples = true;
        MOONMIC_LOG("[audio_capture_vita] First 10 raw int16: %d %d %d %d %d %d %d %d %d %d",
                    data->temp_buffer[0], data->temp_buffer[1], data->temp_buffer[2],
                    data->temp_buffer[3], data->temp_buffer[4], data->temp_buffer[5],
                    data->temp_buffer[6], data->temp_buffer[7], data->temp_buffer[8],
                    data->temp_buffer[9]);
    }
    
    // Convert SIGNED S16 to float32 [-1.0, 1.0]
    // Format is signed: -32768 to 32767, 0 = silence
    for (size_t i = 0; i < frames * data->channels; i++) {
        buffer[i] = static_cast<float>(data->temp_buffer[i]) / 32768.0f;
    }
    
    return frames;
}

static void vita_audio_close(audio_capture_t* self) {
    if (!self || !self->platform_data) {
        return;
    }
    
    vita_audio_data_t* data = (vita_audio_data_t*)self->platform_data;
    
    if (data->port >= 0) {
        sceAudioInReleasePort(data->port);
    }
    
    if (data->temp_buffer) {
        free(data->temp_buffer);
    }
    
    free(data);
    self->platform_data = NULL;
}

static uint32_t vita_audio_get_native_sample_rate(audio_capture_t* self) {
    (void)self;  // Unused
    return PLATFORM_SAMPLE_RATE;
}

audio_capture_t* audio_capture_create_vita(void) {
    audio_capture_t* capture = (audio_capture_t*)calloc(1, sizeof(audio_capture_t));
    if (!capture) {
        return NULL;
    }
    
    
    capture->init = vita_audio_init;
    capture->get_native_sample_rate = vita_audio_get_native_sample_rate;
    capture->read = vita_audio_read;
    capture->close = vita_audio_close;
    capture->platform_data = NULL;
    
    return capture;
}
