#include "core/Pointer.hpp"

namespace screencam::core {

PointerPlacement placePointer(PointD mouse, RectD target, double pointerHeightPoints) {
    PointerPlacement placement;
    if (target.width <= 0.0 || target.height <= 0.0) {
        return placement;
    }
    placement.tip = PointD{(mouse.x - target.x) / target.width, (mouse.y - target.y) / target.height};
    placement.height = pointerHeightPoints / target.height;
    // Only while the mouse is over the recorded area: on another monitor (e.g. the presenter
    // view while the slides are recorded), it isn't part of this video.
    placement.visible = placement.tip.x >= 0.0 && placement.tip.x < 1.0 && placement.tip.y >= 0.0 &&
                        placement.tip.y < 1.0;
    return placement;
}

}  // namespace screencam::core
