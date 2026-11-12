#include "media/BubbleStyleSnapshot.hpp"

#include <algorithm>

namespace {
constexpr double kReferenceHeight = 1080.0;  // borderWidth is expressed for this canvas height
}  // namespace

double BubbleStyleSnapshot::borderPixels(double canvasWidth, double canvasHeight) const {
    return borderWidth * std::min(canvasWidth, canvasHeight) / kReferenceHeight;
}
