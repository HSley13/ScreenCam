#include "core/BubbleGeometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace screencam::core {

namespace {

constexpr double kMinSizeFraction = 0.08;
constexpr double kMaxSizeFraction = 0.6;
constexpr double kRectangleBoost = 1.25;
constexpr double kMarginFraction = 0.03;

double shortSide(SizeD size) {
    return std::min(size.width, size.height);
}

}  // namespace

std::string_view bubbleShapeLabel(BubbleShape shape) {
    switch (shape) {
    case BubbleShape::Circle:
        return "Circle";
    case BubbleShape::Squircle:
        return "Squircle";
    case BubbleShape::Portrait:
        return "Portrait";
    case BubbleShape::Landscape:
        return "Landscape";
    }
    return "Circle";
}

double bubbleAspect(BubbleShape shape) {
    switch (shape) {
    case BubbleShape::Portrait:
        return 9.0 / 16.0;
    case BubbleShape::Landscape:
        return 16.0 / 9.0;
    case BubbleShape::Circle:
    case BubbleShape::Squircle:
        break;
    }
    return 1.0;
}

SizeD bubbleSize(SizeD canvas, BubbleShape shape, double sizeFraction) {
    const double fraction = std::clamp(sizeFraction, kMinSizeFraction, kMaxSizeFraction);
    const double base = shortSide(canvas) * fraction;

    switch (shape) {
    case BubbleShape::Portrait: {
        const double height = base * kRectangleBoost;
        return {height * bubbleAspect(shape), height};
    }
    case BubbleShape::Landscape: {
        const double width = base * kRectangleBoost;
        return {width, width / bubbleAspect(shape)};
    }
    case BubbleShape::Circle:
    case BubbleShape::Squircle:
        break;
    }
    return {base, base};
}

double bubbleCornerRadius(BubbleShape shape, SizeD bubble, double roundness) {
    const double maxRadius = shortSide(bubble) / 2.0;
    if (shape == BubbleShape::Circle) {
        return maxRadius;
    }
    return maxRadius * std::clamp(roundness, 0.0, 1.0);
}

double bubbleMargin(SizeD canvas) {
    return shortSide(canvas) * kMarginFraction;
}

PointD clampNormalizedCenter(SizeD canvas, SizeD bubble, PointD normalizedCenter) {
    if (canvas.width <= 0.0 || canvas.height <= 0.0) {
        return {0.5, 0.5};
    }
    const double margin = bubbleMargin(canvas);

    // Valid range for the centre, in pixels. If the bubble is bigger than the canvas allows,
    // the range collapses and the bubble is simply centred on that axis.
    auto clampAxis = [margin](double normalized, double canvasLength, double bubbleLength) {
        const double minCenter = margin + bubbleLength / 2.0;
        const double maxCenter = canvasLength - margin - bubbleLength / 2.0;
        if (minCenter > maxCenter) {
            return 0.5;
        }
        const double pixels = std::clamp(normalized * canvasLength, minCenter, maxCenter);
        return pixels / canvasLength;
    };

    return {clampAxis(normalizedCenter.x, canvas.width, bubble.width),
            clampAxis(normalizedCenter.y, canvas.height, bubble.height)};
}

RectD bubbleRect(SizeD canvas, BubbleShape shape, double sizeFraction, PointD normalizedCenter) {
    const SizeD size = bubbleSize(canvas, shape, sizeFraction);
    const PointD center = clampNormalizedCenter(canvas, size, normalizedCenter);
    return {center.x * canvas.width - size.width / 2.0, center.y * canvas.height - size.height / 2.0, size.width,
            size.height};
}

PointD snapNormalizedCenter(SizeD canvas, SizeD bubble, PointD normalizedCenter, double threshold) {
    if (canvas.width <= 0.0 || canvas.height <= 0.0) {
        return {0.5, 0.5};
    }
    const double margin = bubbleMargin(canvas);

    // Anchors on one axis: touching the start edge, centred, touching the end edge.
    auto snapAxis = [&](double normalized, double canvasLength, double bubbleLength) {
        const std::array<double, 3> anchors{
            (margin + bubbleLength / 2.0) / canvasLength,
            0.5,
            (canvasLength - margin - bubbleLength / 2.0) / canvasLength,
        };
        double best = normalized;
        double bestDistance = threshold;
        for (const double anchor : anchors) {
            const double distance = std::abs(normalized - anchor);
            if (distance <= bestDistance) {
                best = anchor;
                bestDistance = distance;
            }
        }
        return best;
    };

    const PointD snapped{snapAxis(normalizedCenter.x, canvas.width, bubble.width),
                         snapAxis(normalizedCenter.y, canvas.height, bubble.height)};
    return clampNormalizedCenter(canvas, bubble, snapped);
}

RectD aspectFillCrop(SizeD source, double targetAspect) {
    if (source.width <= 0.0 || source.height <= 0.0 || targetAspect <= 0.0) {
        return {};
    }
    const double sourceAspect = source.width / source.height;
    if (sourceAspect > targetAspect) {
        // Source is wider: keep the full height, cut the sides.
        const double width = source.height * targetAspect;
        return {(source.width - width) / 2.0, 0.0, width, source.height};
    }
    // Source is taller (or equal): keep the full width, cut top and bottom.
    const double height = source.width / targetAspect;
    return {0.0, (source.height - height) / 2.0, source.width, height};
}

RectD aspectFitRect(SizeD source, SizeD target) {
    if (source.width <= 0.0 || source.height <= 0.0 || target.width <= 0.0 || target.height <= 0.0) {
        return {};
    }
    const double scale = std::min(target.width / source.width, target.height / source.height);
    const double width = source.width * scale;
    const double height = source.height * scale;
    return {(target.width - width) / 2.0, (target.height - height) / 2.0, width, height};
}

}  // namespace screencam::core
