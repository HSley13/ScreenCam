#pragma once

// RGB -> NV12 conversion for the video encoder.
//
// Why: H.264 in MP4 is only universally playable (QuickTime, Windows, browsers, phones) as
// 4:2:0 YUV. If we hand the encoder RGB frames, FFmpeg's x264 happily picks 4:4:4, which
// QuickTime refuses to play. So every composited frame is converted to NV12 here:
//   - plane 0: one Y (brightness) byte per pixel,
//   - plane 1: one interleaved (U, V) byte pair per 2x2 block of pixels.
// Colours use BT.709 "video range" (Y 16..235, UV 16..240), the HD standard.

#include <cstdint>

namespace screencam::core {

struct YuvPixel {
    std::uint8_t y;
    std::uint8_t u;
    std::uint8_t v;
};

/// BT.709 video-range conversion of a single colour (exposed for tests and clarity).
YuvPixel rgbToYuv709(int r, int g, int b);

/// Converts a 32-bit-per-pixel image to NV12.
/// `src` holds pixels as native 32-bit words 0xAARRGGBB (Qt's QImage::Format_RGB32/ARGB32
/// layout; alpha is ignored). Strides are in bytes. Width/height should be even; for odd
/// sizes the last row/column is still converted (chroma reuses the edge pixel).
void convertXrgbToNv12(const std::uint8_t *src, int srcStride, int width, int height, std::uint8_t *yPlane,
                       int yStride, std::uint8_t *uvPlane, int uvStride);

}  // namespace screencam::core
