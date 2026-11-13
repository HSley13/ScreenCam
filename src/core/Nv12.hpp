#pragma once

// RGB -> NV12 (BT.709 video range) for the encoder. H.264 in MP4 only plays everywhere as
// 4:2:0; given RGB frames, x264 picks 4:4:4, which QuickTime refuses to play.

#include <cstdint>

namespace screencam::core {

struct YuvPixel {
    std::uint8_t y;
    std::uint8_t u;
    std::uint8_t v;
};

YuvPixel rgbToYuv709(int r, int g, int b);

/// `src`: 0xAARRGGBB words (QImage::Format_RGB32/ARGB32, alpha ignored); strides in bytes.
void convertXrgbToNv12(const std::uint8_t *src, int srcStride, int width, int height, std::uint8_t *yPlane,
                       int yStride, std::uint8_t *uvPlane, int uvStride);

}  // namespace screencam::core
