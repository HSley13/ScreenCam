#pragma once

#include <array>
#include <string_view>

namespace screencam::core {

/// A size in *physical pixels* (not macOS "points": a Retina point is 2x2 pixels).
struct PixelSize {
    int width = 0;
    int height = 0;

    friend constexpr bool operator==(const PixelSize&, const PixelSize&) = default;
};

/// The number is the output height; the width follows the display's aspect ratio.
enum class ResolutionPreset {
    HD720,
    FullHD1080,
    QHD1440,
    UHD2160,
    Native,  // the display's real pixel size (e.g. 3456 x 2234 on a 16" MacBook Pro)
};

struct ResolutionOption {
    ResolutionPreset preset;
    std::string_view label;
    int targetHeight;  // 0 means "use the display's native height"
};

/// In the order the UI lists them.
const std::array<ResolutionOption, 5>& resolutionOptions();

/// Rounds down to an even number (minimum 2): 4:2:0 video needs even dimensions.
int makeEven(int value);

/// Keeps the display's aspect ratio, never upscales, returns even sizes ({0, 0} if invalid).
PixelSize computeCaptureSize(PixelSize displayPixels, ResolutionPreset preset);

}  // namespace screencam::core
