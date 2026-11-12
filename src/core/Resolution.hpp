#pragma once

// Pure C++ (no Apple headers): everything here can be unit-tested on any OS.

#include <array>
#include <string_view>

namespace screencam::core {

/// A size in *physical pixels* (not macOS "points": a Retina point is 2x2 pixels).
struct PixelSize {
    int width = 0;
    int height = 0;

    friend constexpr bool operator==(const PixelSize&, const PixelSize&) = default;
};

/// Resolutions the user can pick for the screen recording.
/// The number is the output *height*; the width follows the display's aspect ratio.
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

/// Every preset, in the order the UI should list them.
const std::array<ResolutionOption, 5>& resolutionOptions();

/// Rounds down to an even number (minimum 2).
/// H.264/HEVC store colour at half resolution (4:2:0), so both dimensions must be even.
int makeEven(int value);

/// Output size of the screen recording for a display of `displayPixels`:
///  - keeps the display's aspect ratio (no black bars, no stretching),
///  - never upscales (asking for 4K on a 1080p display gives 1080p),
///  - always returns even dimensions, or {0, 0} for an invalid display size.
PixelSize computeCaptureSize(PixelSize displayPixels, ResolutionPreset preset);

}  // namespace screencam::core
