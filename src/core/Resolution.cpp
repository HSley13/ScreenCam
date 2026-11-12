#include "core/Resolution.hpp"

#include <cmath>

namespace screencam::core {

const std::array<ResolutionOption, 5>& resolutionOptions() {
    static const std::array<ResolutionOption, 5> options{{
        {ResolutionPreset::HD720, "720p", 720},
        {ResolutionPreset::FullHD1080, "1080p", 1080},
        {ResolutionPreset::QHD1440, "1440p", 1440},
        {ResolutionPreset::UHD2160, "4K (2160p)", 2160},
        {ResolutionPreset::Native, "Native (Retina)", 0},
    }};
    return options;
}

int makeEven(int value) {
    if (value < 2) {
        return 2;
    }
    return value - (value % 2);
}

PixelSize computeCaptureSize(PixelSize displayPixels, ResolutionPreset preset) {
    if (displayPixels.width <= 0 || displayPixels.height <= 0) {
        return {};
    }

    int targetHeight = 0;
    for (const ResolutionOption& option : resolutionOptions()) {
        if (option.preset == preset) {
            targetHeight = option.targetHeight;
            break;
        }
    }

    // Native, or a preset at least as tall as the display: record at native size.
    if (targetHeight == 0 || targetHeight >= displayPixels.height) {
        return {makeEven(displayPixels.width), makeEven(displayPixels.height)};
    }

    // Scale the width by the same factor as the height to keep the aspect ratio.
    const double scale = static_cast<double>(targetHeight) / displayPixels.height;
    const int width = static_cast<int>(std::lround(displayPixels.width * scale));
    return {makeEven(width), makeEven(targetHeight)};
}

}  // namespace screencam::core
