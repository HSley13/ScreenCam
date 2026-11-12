#include "core/CameraLayout.hpp"

#include <cmath>

namespace screencam::core {

std::string_view orientationLabel(CameraOrientation orientation) {
    if (orientation == CameraOrientation::Portrait) {
        return "Portrait";
    }
    return "Landscape";
}

PixelSize cameraRecordingSize(PixelSize cameraPixels, CameraOrientation orientation) {
    if (cameraPixels.width <= 0 || cameraPixels.height <= 0) {
        return {};
    }

    // Wanted aspect ratio expressed as aspectW : aspectH.
    const int aspectW = (orientation == CameraOrientation::Landscape) ? 16 : 9;
    const int aspectH = (orientation == CameraOrientation::Landscape) ? 9 : 16;

    // 1) Try to use the full camera height and derive the width from it.
    int height = cameraPixels.height;
    int width = static_cast<int>(std::lround(static_cast<double>(height) * aspectW / aspectH));

    // 2) Too wide for the camera (e.g. landscape out of a 4:3 camera)? Use the full width instead.
    if (width > cameraPixels.width) {
        width = cameraPixels.width;
        height = static_cast<int>(std::lround(static_cast<double>(width) * aspectH / aspectW));
    }

    return {makeEven(width), makeEven(height)};
}

}  // namespace screencam::core
