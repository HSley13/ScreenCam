#pragma once

#include "core/Resolution.hpp"

namespace screencam::core {

enum class VideoCodec { H264, HEVC };

/// What the video encoder should be asked for, derived only from size and frame rate.
struct EncoderPlan {
    VideoCodec codec = VideoCodec::H264;
    int averageBitRate = 0;       // bits per second
    int maxKeyFrameInterval = 0;  // in frames (a key frame every 2 s makes seeking snappy)
    int framesPerSecond = 30;
};

/// Apple's H.264 encoder tops out around 4096 x 2304. Bigger frames (5K/6K displays at
/// "Native") need HEVC. Works for portrait sizes too by comparing long and short sides.
bool requiresHevc(PixelSize size);

/// Picks codec and bit rate. Bit rate = pixels/s * bits-per-pixel, clamped to a sane range
/// (2-60 Mbit/s). HEVC gets fewer bits per pixel because it compresses ~30% better.
EncoderPlan planEncoder(PixelSize size, int framesPerSecond);

}  // namespace screencam::core
