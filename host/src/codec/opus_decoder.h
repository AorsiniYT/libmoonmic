/**
 * @file opus_decoder.h
 * @brief Opus decoder header
 */

#pragma once

#include <stdint.h>

namespace moonmic {

class OpusDecoder {
public:
    OpusDecoder();
    ~OpusDecoder();
    
    bool init(int sample_rate, int channels);
    bool reinit(int sample_rate, int channels);  // Recreate decoder with new parameters
    int decode(const uint8_t* input, int input_size, float* output, int max_frames);
    
private:
    void* decoder_;  // OpusDecoder*
    int sample_rate_;
    int channels_;
};

} // namespace moonmic
