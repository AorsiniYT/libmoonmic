/**
 * @file ffmpeg_decoder.h
 * @brief FFmpeg-based Opus decoder for moonmic-host
 */

#pragma once

#include <cstdint>

// Forward declarations for FFmpeg types
struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwrContext;

namespace moonmic {

class FFmpegDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder();
    
    /**
     * Initialize decoder for Opus codec
     * @param sample_rate Output sample rate (e.g., 16000 or 48000)
     * @param channels Number of channels (1=mono, 2=stereo)
     * @return true on success
     */
    bool init(int sample_rate, int channels);
    
    /**
     * Reinitialize decoder with new parameters
     * @param sample_rate New sample rate
     * @param channels New channel count
     * @return true on success
     */
    bool reinit(int sample_rate, int channels);
    
    /**
     * Decode Opus packet to float PCM
     * @param input Input Opus packet data
     * @param input_size Size of input packet in bytes
     * @param output Output buffer for float samples (interleaved)
     * @param max_frames Maximum frames to decode
     * @return Number of frames decoded, or -1 on error
     */
    int decode(const uint8_t* input, int input_size, float* output, int max_frames);
    
private:
    const AVCodec* codec_;
    AVCodecContext* codec_ctx_;
    AVFrame* frame_;
    AVPacket* packet_;
    SwrContext* swr_ctx_;
    
    int sample_rate_;
    int channels_;
    
    void cleanup();
};

} // namespace moonmic
