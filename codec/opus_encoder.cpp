/**
 * @file opus_encoder.cpp
 * @brief Opus encoder wrapper for libmoonmic
 */

#include "moonmic_internal.h"
#include <opus/opus.h>
#include <stdlib.h>

moonmic_opus_encoder_t* moonmic_opus_encoder_create(uint32_t sample_rate, uint8_t channels, uint32_t bitrate) {
    moonmic_opus_encoder_t* enc = (moonmic_opus_encoder_t*)calloc(1, sizeof(moonmic_opus_encoder_t));
    if (!enc) {
        return NULL;
    }
    
    int error;
    enc->encoder = opus_encoder_create(
        sample_rate,
        channels,
        OPUS_APPLICATION_VOIP,  // Low latency mode
        &error
    );
    
    if (error != OPUS_OK || !enc->encoder) {
        free(enc);
        return NULL;
    }
    
    // Configure for low latency
    opus_encoder_ctl((OpusEncoder*)enc->encoder, OPUS_SET_BITRATE(bitrate));
    opus_encoder_ctl((OpusEncoder*)enc->encoder, OPUS_SET_VBR(0));  // CBR for consistent latency
    opus_encoder_ctl((OpusEncoder*)enc->encoder, OPUS_SET_COMPLEXITY(5));  // Balance quality/CPU
    opus_encoder_ctl((OpusEncoder*)enc->encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl((OpusEncoder*)enc->encoder, OPUS_SET_DTX(0));  // Disable discontinuous transmission
    
    enc->sample_rate = sample_rate;
    enc->channels = channels;
    enc->bitrate = bitrate;
    
    return enc;
}

void moonmic_opus_encoder_destroy(moonmic_opus_encoder_t* encoder) {
    if (!encoder) {
        return;
    }
    
    if (encoder->encoder) {
        opus_encoder_destroy((OpusEncoder*)encoder->encoder);
    }
    
    free(encoder);
}

int moonmic_opus_encoder_encode(moonmic_opus_encoder_t* encoder, const float* pcm, int frame_size, 
                       uint8_t* output, int max_output_bytes) {
    if (!encoder || !encoder->encoder || !pcm || !output) {
        return -1;
    }
    
    return opus_encode_float(
        (OpusEncoder*)encoder->encoder,
        pcm,
        frame_size,
        output,
        max_output_bytes
    );
}
