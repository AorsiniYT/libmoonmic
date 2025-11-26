/**
 * @file audio_capture_vita.cpp
 * @brief PS Vita audio capture implementation using SceAudioIn
 */

#include "../moonmic_internal.h"
#include "../moonmic_debug.h"
#include <psp2/audioin.h>
#include <psp2/kernel/threadmgr.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int port;
    int grain;  // Samples per read (256 @ 16kHz)
    uint8_t channels;
    int16_t* temp_buffer;  // For S16 samples from mic
} vita_audio_data_t;

static bool vita_audio_init(audio_capture_t* self, uint32_t sample_rate, uint8_t channels) {
    MOONMIC_LOG("[audio_capture_vita] Init called: %uHz, %dch\n", sample_rate, channels);
    
    vita_audio_data_t* data = (vita_audio_data_t*)calloc(1, sizeof(vita_audio_data_t));
    if (!data) {
        MOONMIC_LOG("[audio_capture_vita] ERROR: Failed to allocate data\n");
        return false;
    }
    
    // PS Vita only supports mono microphone input
    if (channels > 1) {
        channels = 1;  // Force mono
        MOONMIC_LOG("[audio_capture_vita] Forcing mono (channels=1)\n");
    }
    
    data->channels = channels;
    
    // PS Vita microphone: 16kHz, grain MUST be 256
    // (160, 320 fail with 0x80260102)
    uint32_t actual_sample_rate = 16000;
    data->grain = 256;
    
    MOONMIC_LOG("[audio_capture_vita] Using 16kHz, grain=256 (Vita hardware requirement)");
    
    MOONMIC_LOG("[audio_capture_vita] Opening audio port: grain=%d, freq=%u, param=%d\n", 
        data->grain, actual_sample_rate, SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO);
    
    // Open audio input port
    // sceAudioInOpenPort(portType, grain, freq, param)
    SceAudioInParam param = SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO;
    
    data->port = sceAudioInOpenPort(
        SCE_AUDIO_IN_PORT_TYPE_VOICE,
        data->grain,
        actual_sample_rate,
        param
    );
    
    if (data->port < 0) {
        MOONMIC_LOG("[audio_capture_vita] ERROR: sceAudioInOpenPort failed: 0x%08X\n", data->port);
        free(data);
        return false;
    }
    
    MOONMIC_LOG("[audio_capture_vita] Audio port opened successfully: %d\n", data->port);
    
    // Allocate temporary buffer for S16 samples (16kHz)
    data->temp_buffer = (int16_t*)malloc(data->grain * channels * sizeof(int16_t));
    if (!data->temp_buffer) {
        MOONMIC_LOG("[audio_capture_vita] ERROR: Failed to allocate temp buffer\n");
        sceAudioInReleasePort(data->port);
        free(data);
        return false;
    }
    
    self->platform_data = data;
    MOONMIC_LOG("[audio_capture_vita] Init completed successfully (16kHz direct capture)\n");
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
    
    // Convert S16 to float32 [-1.0, 1.0]
    for (size_t i = 0; i < frames * data->channels; i++) {
        buffer[i] = data->temp_buffer[i] / 32768.0f;
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

audio_capture_t* audio_capture_create_vita(void) {
    audio_capture_t* capture = (audio_capture_t*)calloc(1, sizeof(audio_capture_t));
    if (!capture) {
        return NULL;
    }
    
    capture->init = vita_audio_init;
    capture->read = vita_audio_read;
    capture->close = vita_audio_close;
    capture->platform_data = NULL;
    
    return capture;
}
