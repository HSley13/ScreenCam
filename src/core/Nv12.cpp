#include "core/Nv12.hpp"

#include <algorithm>
#include <cstring>

namespace screencam::core {

namespace {

// 8.8 fixed-point BT.709 coefficients (value * 256, rounded):
//   Y =  0.1826 R + 0.6142 G + 0.0620 B + 16
//   U = -0.1006 R - 0.3386 G + 0.4392 B + 128
// Each chroma row is rounded so its coefficients sum to exactly 0: then any grey (R = G = B)
// maps to U = V = 128 — no colour tint on white text or grey UI.
//   V =  0.4392 R - 0.3989 G - 0.0403 B + 128
// ">> 8" on a negative int is an arithmetic shift, which C++20 guarantees.
inline int lumaOf(int r, int g, int b) {
    return 16 + ((47 * r + 157 * g + 16 * b + 128) >> 8);
}

inline int chromaUOf(int r, int g, int b) {
    return 128 + ((-26 * r - 86 * g + 112 * b + 128) >> 8);
}

inline int chromaVOf(int r, int g, int b) {
    return 128 + ((112 * r - 102 * g - 10 * b + 128) >> 8);
}

inline std::uint8_t toByte(int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

inline std::uint32_t pixelAt(const std::uint8_t *src, int srcStride, int x, int y) {
    std::uint32_t pixel = 0;
    std::memcpy(&pixel, src + static_cast<std::ptrdiff_t>(y) * srcStride + static_cast<std::ptrdiff_t>(x) * 4,
                sizeof(pixel));
    return pixel;
}

inline int redOf(std::uint32_t p) {
    return static_cast<int>((p >> 16) & 0xFF);
}

inline int greenOf(std::uint32_t p) {
    return static_cast<int>((p >> 8) & 0xFF);
}

inline int blueOf(std::uint32_t p) {
    return static_cast<int>(p & 0xFF);
}

}  // namespace

YuvPixel rgbToYuv709(int r, int g, int b) {
    return {toByte(lumaOf(r, g, b)), toByte(chromaUOf(r, g, b)), toByte(chromaVOf(r, g, b))};
}

void convertXrgbToNv12(const std::uint8_t *src, int srcStride, int width, int height, std::uint8_t *yPlane,
                       int yStride, std::uint8_t *uvPlane, int uvStride) {
    if (src == nullptr || yPlane == nullptr || uvPlane == nullptr || width <= 0 || height <= 0) {
        return;
    }

    // Pass 1: full-resolution luma.
    for (int y = 0; y < height; y++) {
        std::uint8_t *yRow = yPlane + static_cast<std::ptrdiff_t>(y) * yStride;
        for (int x = 0; x < width; x++) {
            const std::uint32_t p = pixelAt(src, srcStride, x, y);
            yRow[x] = toByte(lumaOf(redOf(p), greenOf(p), blueOf(p)));
        }
    }

    // Pass 2: chroma at half resolution, averaging each 2x2 block (smoother than sampling
    // one pixel, which matters for coloured text and thin UI lines in screen recordings).
    const int chromaWidth = (width + 1) / 2;
    const int chromaHeight = (height + 1) / 2;
    for (int cy = 0; cy < chromaHeight; cy++) {
        std::uint8_t *uvRow = uvPlane + static_cast<std::ptrdiff_t>(cy) * uvStride;
        const int y0 = cy * 2;
        const int y1 = std::min(y0 + 1, height - 1);
        for (int cx = 0; cx < chromaWidth; cx++) {
            const int x0 = cx * 2;
            const int x1 = std::min(x0 + 1, width - 1);
            const std::uint32_t p00 = pixelAt(src, srcStride, x0, y0);
            const std::uint32_t p01 = pixelAt(src, srcStride, x1, y0);
            const std::uint32_t p10 = pixelAt(src, srcStride, x0, y1);
            const std::uint32_t p11 = pixelAt(src, srcStride, x1, y1);
            const int r = (redOf(p00) + redOf(p01) + redOf(p10) + redOf(p11) + 2) / 4;
            const int g = (greenOf(p00) + greenOf(p01) + greenOf(p10) + greenOf(p11) + 2) / 4;
            const int b = (blueOf(p00) + blueOf(p01) + blueOf(p10) + blueOf(p11) + 2) / 4;
            uvRow[cx * 2] = toByte(chromaUOf(r, g, b));
            uvRow[cx * 2 + 1] = toByte(chromaVOf(r, g, b));
        }
    }
}

}  // namespace screencam::core
