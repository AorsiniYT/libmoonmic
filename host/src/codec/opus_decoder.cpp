/**
 * @file opus_decoder.cpp
 * @brief Opus decoder for moonmic-host
 */

#include "opus_decoder.h"
#include <opus/opus.h>
#include <iostream>

namespace moonmic {

OpusDecoder::OpusDecoder()
    : decoder_(nullptr)
    , sample_rate_(0)
    , channels_(0) {
}

OpusDecoder::~OpusDecoder() {
    if (decoder_) {
        opus_decoder_destroy(static_cast<::OpusDecoder*>(decoder_));
    }
}

bool OpusDecoder::init(int sample_rate, int channels) {
    int error;
    decoder_ = opus_decoder_create(sample_rate, channels, &error);
    
    if (error != OPUS_OK || !decoder_) {
        std::cerr << "[OpusDecoder] Failed to create: " << opus_strerror(error) << std::endl;
        return false;
    }
    
    sample_rate_ = sample_rate;
    channels_ = channels;
    
    std::cout << "[OpusDecoder] Initialized: " << sample_rate << "Hz, " 
              << channels << " channels" << std::endl;
    return true;
}

int OpusDecoder::decode(const uint8_t* input, int input_size, float* output, int max_frames) {
    if (!decoder_) {
        return -1;
    }
    
    int frames = opus_decode_float(
        static_cast<::OpusDecoder*>(decoder_),
        input,
        input_size,
        output,
        max_frames,
        0  // decode_fec
    );
    
    if (frames < 0) {
        std::cerr << "[OpusDecoder] Decode error: " << opus_strerror(frames) << std::endl;
        return -1;
    }
    
    return frames;
}

} // namespace moonmic
