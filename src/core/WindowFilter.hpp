#pragma once

// Which windows "Record ▸ Window" offers. The OS window list also holds menu-bar items, the
// Dock, panels, tool palettes and hidden helpers; only real app windows are kept. Each
// platform fills WindowTraits (src/app/AppWindows*); the decision is made here, in one place.

#include <string>

namespace screencam::core {

struct WindowTraits {
    bool visible = true;
    /// macOS kCGWindowLayer: app windows are 0, the menu bar, Dock and pop-ups higher.
    int layer = 0;
    double alpha = 1.0;
    int width = 0;
    int height = 0;
    bool ownedBySelf = false;
    /// Has a Dock/taskbar presence (macOS: NSApplicationActivationPolicyRegular).
    bool regularApp = true;
    /// Windows WS_EX_TOOLWINDOW, X11 _NET_WM_WINDOW_TYPE_DOCK, owned pop-ups, ...
    bool toolWindow = false;
    /// Windows DWMWA_CLOAKED: on another virtual desktop, or a suspended Store app.
    bool cloaked = false;
    bool hasWindowControls = true;
};

/// Smaller than any real app window, bigger than status items and tooltips.
inline constexpr int kMinAppWindowWidth = 120;
inline constexpr int kMinAppWindowHeight = 80;

bool isUserAppWindow(const WindowTraits &window);

/// "Safari — GitHub"; just the app name when the title is empty (macOS hides titles until
/// Screen Recording is allowed) or repeats it.
std::string windowLabel(const std::string &appName, const std::string &title);

}  // namespace screencam::core
