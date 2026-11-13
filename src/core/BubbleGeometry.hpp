#pragma once

// Camera bubble geometry, in canvas pixels. Shared by the QML preview and the compositor, so
// what you place in the preview is exactly what ends up in the video.

#include <string_view>

namespace screencam::core {

/// Circle, squircle (rounded square), portrait (9:16) and landscape (16:9).
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

/// `sizeFraction` (clamped to [0.08, 0.6]) is relative to the canvas's shorter side, so the
/// bubble looks the same on landscape and portrait canvases. Rectangles get a 25% longer long
/// side so they don't look smaller than a circle.
SizeD bubbleSize(SizeD canvas, BubbleShape shape, double sizeFraction);

/// Circles are fully round; other shapes use `roundness` (0..1, 1 = half the short side).
double bubbleCornerRadius(BubbleShape shape, SizeD bubble, double roundness);

/// 3% of the canvas's shorter side.
double bubbleMargin(SizeD canvas);

/// The bubble for a normalized centre (0..1), kept inside the canvas margin.
RectD bubbleRect(SizeD canvas, BubbleShape shape, double sizeFraction, PointD normalizedCenter);

/// Normalized centre after clamping the bubble fully inside the canvas.
PointD clampNormalizedCenter(SizeD canvas, SizeD bubble, PointD normalizedCenter);

/// On drop, each axis snaps to the nearest edge or the middle when within `threshold`
/// (a fraction of the canvas size).
PointD snapNormalizedCenter(SizeD canvas, SizeD bubble, PointD normalizedCenter, double threshold);

/// Centred crop of `source` with aspect `targetAspect` (w / h): fills without stretching.
RectD aspectFillCrop(SizeD source, double targetAspect);

/// Largest centred rectangle with `source`'s aspect that fits in `target` (letterboxing).
RectD aspectFitRect(SizeD source, SizeD target);

}  // namespace screencam::core
