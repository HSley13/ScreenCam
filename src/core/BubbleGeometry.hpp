#pragma once

// Geometry of the camera bubble, in canvas pixels. Pure C++ so the QML preview (which shows
// the bubble while you drag it) and the compositor (which paints it into the video) share one
// implementation: what you place in the preview is exactly what ends up in the recording.

#include <string_view>

namespace screencam::core {

/// The four frame shapes Loom offers: a circle, a "squircle" (rounded square) and two
/// rectangles — portrait (9:16) and landscape (16:9).
enum class BubbleShape {
    Circle,
    Squircle,
    Portrait,
    Landscape,
};

std::string_view bubbleShapeLabel(BubbleShape shape);

struct SizeD {
    double width = 0.0;
    double height = 0.0;
};

struct PointD {
    double x = 0.0;
    double y = 0.0;
};

struct RectD {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    PointD center() const { return {x + width / 2.0, y + height / 2.0}; }
};

/// Width / height of the shape (1 for circle and squircle).
double bubbleAspect(BubbleShape shape);

/// Size of the bubble on a canvas. `sizeFraction` (clamped to [0.08, 0.6]) is the bubble's
/// size relative to the canvas's *shorter* side, so the bubble looks equally big on a
/// landscape screen recording and a portrait one. Rectangular shapes are 25% longer on their
/// long side than a circle of the same size fraction so they don't look smaller.
SizeD bubbleSize(SizeD canvas, BubbleShape shape, double sizeFraction);

/// Corner radius in pixels. A circle is always fully round; the other shapes use
/// `roundness` (clamped to [0, 1]) where 1 means "as round as possible" (half the short side).
double bubbleCornerRadius(BubbleShape shape, SizeD bubble, double roundness);

/// Margin kept between the bubble and the canvas edge: 3% of the canvas's shorter side.
double bubbleMargin(SizeD canvas);

/// The bubble rectangle for a *normalized* centre (0..1 on each axis). The rectangle is moved
/// back inside the canvas (respecting the margin) if the centre would push it outside.
RectD bubbleRect(SizeD canvas, BubbleShape shape, double sizeFraction, PointD normalizedCenter);

/// Normalized centre after clamping the bubble fully inside the canvas.
PointD clampNormalizedCenter(SizeD canvas, SizeD bubble, PointD normalizedCenter);

/// Snapping used when the user drops the bubble: on each axis the centre jumps to the nearest
/// "anchor" (left edge / middle / right edge, top / middle / bottom) if it is within
/// `threshold` (a fraction of the canvas size) of it; otherwise it stays where it was dropped.
PointD snapNormalizedCenter(SizeD canvas, SizeD bubble, PointD normalizedCenter, double threshold);

/// The centred sub-rectangle of `source` that has the aspect ratio `targetAspect` (w / h).
/// Used to crop the camera image so it fills the bubble without being stretched.
RectD aspectFillCrop(SizeD source, double targetAspect);

}  // namespace screencam::core
