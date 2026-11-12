#include "core/WindowFilter.hpp"

namespace screencam::core {

bool isUserAppWindow(const WindowTraits &window) {
    if (!window.visible || window.cloaked || window.ownedBySelf) {
        return false;
    }
    // Menu bar, status items, Dock, overlays: anything not on the normal window level.
    if (window.layer != 0) {
        return false;
    }
    if (window.alpha <= 0.0) {
        return false;
    }
    if (!window.regularApp || window.toolWindow || !window.hasWindowControls) {
        return false;
    }
    return window.width >= kMinAppWindowWidth && window.height >= kMinAppWindowHeight;
}

std::string windowLabel(const std::string &appName, const std::string &title) {
    if (title.empty() || title == appName) {
        return appName;
    }
    if (appName.empty()) {
        return title;
    }
    return appName + " — " + title;  // em dash
}

}  // namespace screencam::core
