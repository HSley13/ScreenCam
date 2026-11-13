#pragma once

// Where to draw the mouse pointer in the video. ScreenCam draws it itself: most capture
// methods leave it out, and apps that hide it (PowerPoint's slide show) would hide it from
// the video too.

#include "core/BubbleGeometry.hpp"

namespace screencam::core {

/// In desktop points, about the size of the macOS arrow.
inline constexpr double kPointerHeightPoints = 22.0;

struct PointerPlacement {
    bool visible = false;  // the mouse is over the recorded screen/window
    PointD tip;            // tip of the arrow, normalized to the captured frame (0..1)
    double height = 0.0;   // pointer height as a fraction of the captured frame's height
};

/// `mouse` and `target` (the captured screen or window) in the same desktop coordinates.
PointerPlacement placePointer(PointD mouse, RectD target, double pointerHeightPoints = kPointerHeightPoints);

}  // namespace screencam::core
