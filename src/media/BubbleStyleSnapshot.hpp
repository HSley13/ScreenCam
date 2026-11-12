#pragma once

#include "core/BubbleGeometry.hpp"

#include <QColor>

/// Plain copy of the bubble's look and position: safe to copy to the compositor thread.
struct BubbleStyleSnapshot {
    screencam::core::BubbleShape shape = screencam::core::BubbleShape::Circle;
    double size = 0.22;
    double roundness = 0.35;
    bool mirrored = true;
    double borderWidth = 4.0;  // in pixels of a 1080-pixel-tall canvas, scaled to the real one
    QColor borderColor = Qt::white;
    bool shadow = true;
    screencam::core::PointD center{0.12, 0.84};

    /// Border width in pixels for a canvas of this size.
    double borderPixels(double canvasWidth, double canvasHeight) const;
};
