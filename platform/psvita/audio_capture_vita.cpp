/**
 * @file audio_capture_vita.cpp
 * @brief PS Vita audio capture implementation using SceAudioIn
 */

#include "../moonmic_internal.h"
#include <psp2/audioin.h>
#include <psp2/kernel/threadmgr.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int port;
    int grain;  // Samples per read
    uint8_t channels;
    int16_t* temp_buffer;  // For S16 samples
} vita_audio_data_t;

static bool vita_audio_init(audio_capture_t* self, uint32_t sample_rate, uint8_t channels) {
    vita_audio_data_t* data = (vita_audio_data_t*)calloc(1, sizeof(vita_audio_data_t));
    if (!data) {
        return false;
    }
    
    // PS Vita only supports mono microphone input
    if (channels > 1) {
        channels = 1;  // Force mono
    }
    
    data->channels = channels;
    data->grain = 480;  // 10ms at 48kHz
    
    // Open audio input port
    // sceAudioInOpenPort(portType, grain, freq, param)
    SceAudioInParam param = SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO;
    
    data->port = sceAudioInOpenPort(
        SCE_AUDIO_IN_PORT_TYPE_VOICE,
        data->grain,
        sample_rate,
        param
    );
    
    if (data->port < 0) {
        free(data);
        return false;
    }
    
    // Allocate temporary buffer for S16 samples
    data->temp_buffer = (int16_t*)malloc(data->grain * channels * sizeof(int16_t));
    if (!data->temp_buffer) {
        sceAudioInReleasePort(data->port);
        free(data);
        return false;
    }
    
    self->platform_data = data;
    return true;
}

static int vita_audio_read(audio_capture_t* self, float* buffer, size_t frames) {
    vita_audio_data_t* data = (vita_audio_data_t*)self->platform_data;
    if (!data || !buffer) {
        return -1;
    }
    
    // Limit frames to grain size
    if (frames > (size_t)data->grain) {
        frames = data->grain;
    }
    
    // Read S16 samples from microphone
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
