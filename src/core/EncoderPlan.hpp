#pragma once

#include "core/Resolution.hpp"

namespace screencam::core {

enum class VideoCodec { H264, HEVC };

struct EncoderPlan {
    VideoCodec codec = VideoCodec::H264;
    int averageBitRate = 0;       // bits per second
    int maxKeyFrameInterval = 0;  // in frames (a key frame every 2 s makes seeking snappy)
    int framesPerSecond = 30;
};

/// Apple's H.264 encoder tops out around 4096 x 2304; bigger frames (5K/6K "Native") need HEVC.
bool requiresHevc(PixelSize size);

/// Bit rate = pixels/s * bits-per-pixel, clamped to 2-60 Mbit/s (fewer bits for HEVC).
EncoderPlan planEncoder(PixelSize size, int framesPerSecond);

}  // namespace screencam::core
