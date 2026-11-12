#include "core/EncoderPlan.hpp"

#include <algorithm>

namespace screencam::core {

namespace {
constexpr int kMaxH264LongSide = 4096;
constexpr int kMaxH264ShortSide = 2304;
constexpr double kMinBitRate = 2'000'000.0;
constexpr double kMaxBitRate = 60'000'000.0;
}  // namespace

bool requiresHevc(PixelSize size) {
    const int longSide = std::max(size.width, size.height);
    const int shortSide = std::min(size.width, size.height);
    return longSide > kMaxH264LongSide || shortSide > kMaxH264ShortSide;
}

EncoderPlan planEncoder(PixelSize size, int framesPerSecond) {
    EncoderPlan plan;
    if (framesPerSecond <= 0) {
        framesPerSecond = 30;
    }
    plan.framesPerSecond = framesPerSecond;
    plan.codec = requiresHevc(size) ? VideoCodec::HEVC : VideoCodec::H264;

    const double bitsPerPixel = (plan.codec == VideoCodec::HEVC) ? 0.07 : 0.1;
    const double pixelsPerSecond = static_cast<double>(size.width) * size.height * framesPerSecond;
    plan.averageBitRate = static_cast<int>(std::clamp(pixelsPerSecond * bitsPerPixel, kMinBitRate, kMaxBitRate));
    plan.maxKeyFrameInterval = framesPerSecond * 2;
    return plan;
}

}  // namespace screencam::core
