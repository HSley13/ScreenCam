#pragma once

#include <string_view>

#include "core/Resolution.hpp"

namespace screencam::core {

/// How the separate "camera" recording (you, without the screen) is framed.
enum class CameraOrientation {
    Landscape,  // 16:9, e.g. 1920 x 1080
    Portrait,   // 9:16, e.g. 608 x 1080 cut from the middle of a 1080p webcam
};

std::string_view orientationLabel(CameraOrientation orientation);

/// Largest centred rectangle with the orientation's aspect ratio (16:9 or 9:16) that fits
/// inside a camera frame of `cameraPixels`. This becomes the size of the camera recording;
/// the encoder crops the middle of each frame to it (no stretching, no upscaling).
/// Returns {0, 0} for an invalid camera size.
PixelSize cameraRecordingSize(PixelSize cameraPixels, CameraOrientation orientation);

}  // namespace screencam::core
