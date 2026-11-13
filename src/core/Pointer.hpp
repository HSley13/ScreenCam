#pragma once

// Where to draw the mouse pointer in the video.
//
// Why ScreenCam draws the pointer itself: most capture methods leave it out. Window captures
// on macOS (a picture of one window) and screen captures on Windows (DXGI) and Linux (X11)
// have no pointer at all; ScreenCaptureKit could draw it, but then a presentation app that
// hides the pointer (PowerPoint's slide show does after a few seconds) hides it from the
// video too. Drawing it from the real mouse position makes it always there, the same on
// every platform, and lets it be highlighted.

#include "core/BubbleGeometry.hpp"

namespace screencam::core {

/// Height of the drawn pointer in desktop points (the macOS arrow is about this tall).
inline constexpr double kPointerHeightPoints = 22.0;

struct PointerPlacement {
    bool visible = false;  // the mouse is over the recorded screen/window
    PointD tip;            // tip of the arrow, normalized to the captured frame (0..1)
    double height = 0.0;   // pointer height as a fraction of the captured frame's height
};

/// `mouse`: the mouse position on the desktop; `target`: where the captured screen or window
/// is on the desktop (same coordinates, e.g. points). The captured frame shows exactly
/// `target`, so the pointer's place in it is the same fraction of its width and height.
PointerPlacement placePointer(PointD mouse, RectD target, double pointerHeightPoints = kPointerHeightPoints);

}  // namespace screencam::core
