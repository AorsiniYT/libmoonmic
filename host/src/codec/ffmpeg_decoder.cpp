/**
 * @file ffmpeg_decoder.cpp
 * @brief FFmpeg-based Opus decoder implementation
 */

#include "ffmpeg_decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <iostream>
#include <cstring>

namespace moonmic {

FFmpegDecoder::FFmpegDecoder()
    : codec_(nullptr)
    , codec_ctx_(nullptr)
    , frame_(nullptr)
    , packet_(nullptr)
    , swr_ctx_(nullptr)
    , sample_rate_(0)
    , channels_(0) {
}

FFmpegDecoder::~FFmpegDecoder() {
    cleanup();
}

void FFmpegDecoder::cleanup() {
    if (swr_ctx_) {
        swr_free(&swr_ctx_);
        swr_ctx_ = nullptr;
    }
    
    if (frame_) {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }
    
    if (packet_) {
        av_packet_free(&packet_);
        packet_ = nullptr;
    }
    
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
}

bool FFmpegDecoder::init(int sample_rate, int channels) {
    cleanup();
    
    // Find Opus decoder
    codec_ = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    if (!codec_) {
        std::cerr << "[FFmpegDecoder] Opus codec not found" << std::endl;
        return false;
    }
    
    // Allocate codec context
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        std::cerr << "[FFmpegDecoder] Failed to allocate codec context" << std::endl;
        return false;
    }
    
    // Configure decoder
    codec_ctx_->sample_rate = sample_rate;
    codec_ctx_->ch_layout.nb_channels = channels;
    if (channels == 1) {
        codec_ctx_->ch_layout = AV_CHANNEL_LAYOUT_MONO;
    } else {
        codec_ctx_->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    }
    codec_ctx_->sample_fmt = AV_SAMPLE_FMT_FLT;  // Request float output
    
    // Open codec
    int ret = avcodec_open2(codec_ctx_, codec_, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "[FFmpegDecoder] Failed to open codec: " << errbuf << std::endl;
        cleanup();
        return false;
    }
    
    // Allocate frame and packet
    frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    if (!frame_ || !packet_) {
        std::cerr << "[FFmpegDecoder] Failed to allocate frame/packet" << std::endl;
        cleanup();
        return false;
    }
    
    // Initialize resampler (in case decoder outputs planar format, convert to interleaved)
    swr_ctx_ = swr_alloc();
    if (!swr_ctx_) {
        std::cerr << "[FFmpegDecoder] Failed to allocate resampler" << std::endl;
        cleanup();
        return false;
    }
    
    // Configure resampler: planar float -> interleaved float
    av_opt_set_chlayout(swr_ctx_, "in_chlayout", &codec_ctx_->ch_layout, 0);
    av_opt_set_int(swr_ctx_, "in_sample_rate", sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);  // Planar input
    
    av_opt_set_chlayout(swr_ctx_, "out_chlayout", &codec_ctx_->ch_layout, 0);
    av_opt_set_int(swr_ctx_, "out_sample_rate", sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);  // Interleaved output
    
    ret = swr_init(swr_ctx_);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "[FFmpegDecoder] Failed to initialize resampler: " << errbuf << std::endl;
        cleanup();
        return false;
    }
    
    sample_rate_ = sample_rate;
    channels_ = channels;
    
    std::cout << "[FFmpegDecoder] Initialized: " << sample_rate << "Hz, " 
              << channels << " channels (Opus via FFmpeg)" << std::endl;
    return true;
}

bool FFmpegDecoder::reinit(int sample_rate, int channels) {
    return init(sample_rate, channels);
}

int FFmpegDecoder::decode(const uint8_t* input, int input_size, float* output, int max_frames) {
    if (!codec_ctx_ || !input || !output) {
        return -1;
    }
    
    // DEBUG: Log first few decode calls
    // Set to 100 to disable logging (was 0)
    static int decode_count = 100;
    if (decode_count < 10) {
        std::cout << "[FFmpegDecoder] Decode #" << decode_count 
                  << ": input_size=" << input_size 
                  << " bytes, max_frames=" << max_frames << std::endl;
        std::cout << "[FFmpegDecoder] First 10 input bytes: ";
        for (int i = 0; i < std::min(10, input_size); i++) {
            printf("%02X ", input[i]);
        }
        std::cout << std::endl;
        decode_count++;
    }
    
    // Set packet data
    packet_->data = const_cast<uint8_t*>(input);
    packet_->size = input_size;
    
    // Send packet to decoder
    int ret = avcodec_send_packet(codec_ctx_, packet_);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "[FFmpegDecoder] avcodec_send_packet failed: " << errbuf << std::endl;
        return -1;
    }
    
    // Receive decoded frame
    ret = avcodec_receive_frame(codec_ctx_, frame_);
    if (ret < 0) {
        if (ret != AVERROR(EAGAIN)) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "[FFmpegDecoder] avcodec_receive_frame failed: " << errbuf << std::endl;
        }
        return 0; // Return 0 for EAGAIN or other non-fatal errors where no frame is output
    }
    
    int num_samples = frame_->nb_samples;
    
    if (decode_count <= 10) {
        std::cout << "[FFmpegDecoder] Decoded " << num_samples << " samples" << std::endl;
        std::cout << "[FFmpegDecoder] Frame format: " << frame_->format 
                  << " (FLTP=" << AV_SAMPLE_FMT_FLTP << ", FLT=" << AV_SAMPLE_FMT_FLT << ")" << std::endl;
    }
    
    // Handle both planar and interleaved float formats
    if (frame_->format == AV_SAMPLE_FMT_FLTP) {
        // Planar float - convert to interleaved using SwrContext
        const uint8_t* in_data[AV_NUM_DATA_POINTERS] = {0};
        for (int i = 0; i < codec_ctx_->ch_layout.nb_channels; i++) {
            in_data[i] = frame_->data[i];
        }
        
        uint8_t* out_data[1] = { reinterpret_cast<uint8_t*>(output) };
        int out_samples = max_frames;
        
        ret = swr_convert(swr_ctx_, out_data, out_samples,
                          in_data, num_samples);
        
        if (ret < 0) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "[FFmpegDecoder] swr_convert failed: " << errbuf << std::endl;
            av_frame_unref(frame_); // Unref frame before returning
            return -1;
        }
        
        if (decode_count <= 10) {
            std::cout << "[FFmpegDecoder] Converted " << ret << " samples (planar->interleaved)" << std::endl;
            std::cout << "[FFmpegDecoder] First 5 output samples: " 
                      << output[0] << " " << output[1] << " " << output[2] << " " 
                      << output[3] << " " << output[4] << std::endl;
        }
        
        av_frame_unref(frame_); // Unref frame for next decode
        return ret;
    } else if (frame_->format == AV_SAMPLE_FMT_FLT) {
        // Already interleaved float - direct copy
        int samples_to_copy = std::min(num_samples * codec_ctx_->ch_layout.nb_channels, 
                                       max_frames * codec_ctx_->ch_layout.nb_channels);
        memcpy(output, frame_->data[0], samples_to_copy * sizeof(float));
        
        if (decode_count <= 10) {
            std::cout << "[FFmpegDecoder] Copied " << num_samples << " samples (interleaved)" << std::endl;
            std::cout << "[FFmpegDecoder] First 5 output samples: " 
                      << output[0] << " " << output[1] << " " << output[2] << " " 
                      << output[3] << " " << output[4] << std::endl;
        }
        
        av_frame_unref(frame_); // Unref frame for next decode
        return num_samples;
    } else {
        std::cerr << "[FFmpegDecoder] Unexpected sample format: " << frame_->format << std::endl;
        av_frame_unref(frame_);
        return -1;
    }
}

} // namespace moonmic
