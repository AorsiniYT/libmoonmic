/**
 * @file audio_capture_vita.cpp
 * @brief PS Vita audio capture implementation using SceAudio
 */

#include "../moonmic_internal.h"
#include <psp2/audioout.h>
#include <psp2/audioin.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int port;
    uint32_t sample_rate;
    uint8_t channels;
    int16_t* temp_buffer;  // For S16 to float conversion
    size_t temp_buffer_size;
} vita_audio_data_t;

static bool vita_audio_init(audio_capture_t* self, uint32_t sample_rate, uint8_t channels) {
    vita_audio_data_t* data = (vita_audio_data_t*)calloc(1, sizeof(vita_audio_data_t));
    if (!data) {
        return false;
    }
    
    // PS Vita audio input supports specific configurations
    // We'll use voice input which is optimized for microphone
    SceAudioInParam param;
    param.channelId = SCE_AUDIO_IN_CHANNEL_ID_AUTO;
    param.sampleRate = sample_rate;
    param.grain = 480;  // 10ms at 48kHz
    
    // PS Vita supports mono or stereo
    if (channels == 1) {
        param.param = SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO;
    } else {
        param.param = SCE_AUDIO_IN_PARAM_FORMAT_S16_STEREO;
    }
    
    data->port = sceAudioInOpenPort(SCE_AUDIO_IN_PORT_TYPE_VOICE, &param);
    if (data->port < 0) {
        free(data);
        return false;
    }
    
    data->sample_rate = sample_rate;
    data->channels = channels;
    data->temp_buffer_size = 480 * channels;  // grain * channels
    data->temp_buffer = (int16_t*)malloc(data->temp_buffer_size * sizeof(int16_t));
    
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
    if (!data) {
        return -1;
    }
    
    // Read from audio input (blocking call)
    int ret = sceAudioInInput(data->port, data->temp_buffer);
    if (ret < 0) {
        return -1;
    }
    
    // Convert S16 to float32 [-1.0, 1.0]
    size_t samples = frames * data->channels;
    for (size_t i = 0; i < samples; i++) {
        buffer[i] = data->temp_buffer[i] / 32768.0f;
    }
    
    return frames;
}

static void vita_audio_close(audio_capture_t* self) {
    vita_audio_data_t* data = (vita_audio_data_t*)self->platform_data;
    if (!data) {
        return;
    }
    
    if (data->port >= 0) {
        sceAudioInReleasePort(data->port);
    }
    
    free(data->temp_buffer);
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
