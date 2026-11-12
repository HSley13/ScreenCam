#pragma once

// Which open windows are worth offering in "Record ▸ Window".
//
// Why this exists: the operating system's window list contains far more than app windows.
// On macOS it also has the menu bar, every menu-bar icon (clock, battery, Wi-Fi, CPU meters),
// the Dock, the wallpaper, notification banners and invisible helper windows; on Windows,
// tool palettes, tray pop-ups and windows parked on other virtual desktops; on Linux, panels
// and the desktop itself. Qt passes that whole list through. What you actually want to
// record is a *real app window*: the kind you can minimize, maximize and switch to.
//
// Each platform collects facts about a window into WindowTraits (src/app/AppWindows*); the
// decision is made here, in plain C++, so it is the same everywhere and unit-tested.

#include <string>

namespace screencam::core {

struct WindowTraits {
    bool visible = true;
    /// Stacking level. Normal app windows are on level 0; the menu bar, status items, the
    /// Dock, pop-up menus and overlays sit on higher levels (macOS kCGWindowLayer).
    int layer = 0;
    /// Fully transparent windows exist (event catchers, hidden helpers) and show nothing.
    double alpha = 1.0;
    int width = 0;
    int height = 0;
    /// One of ScreenCam's own windows (never what you want to record).
    bool ownedBySelf = false;
    /// The owning app is a normal app with a Dock/taskbar presence, not a menu-bar utility,
    /// background agent or system process (macOS: NSApplicationActivationPolicyRegular).
    bool regularApp = true;
    /// A tool/utility/dock/desktop-type window, or an owned pop-up (Windows WS_EX_TOOLWINDOW,
    /// X11 _NET_WM_WINDOW_TYPE_DOCK, ...).
    bool toolWindow = false;
    /// Hidden by the window manager although "visible": on another virtual desktop, or a
    /// suspended Store app (Windows DWMWA_CLOAKED).
    bool cloaked = false;
    /// Has a frame the user can minimize/maximize (a title bar, or minimize/maximize boxes).
    bool hasWindowControls = true;
};

/// Smallest window that is still a real app window, in pixels. Status items, cursors and
/// tooltips are smaller; the smallest real windows (a calculator, a mini player) are bigger.
inline constexpr int kMinAppWindowWidth = 120;
inline constexpr int kMinAppWindowHeight = 80;

/// True for windows a person would call "an app's window".
bool isUserAppWindow(const WindowTraits &window);

/// The name shown in the window picker: "Safari — GitHub". Just the app name when the title
/// is empty (macOS hides titles until Screen Recording is allowed) or repeats the app name.
std::string windowLabel(const std::string &appName, const std::string &title);

}  // namespace screencam::core
