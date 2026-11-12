#pragma once

#include <QHash>
#include <QRect>
#include <QString>

/// An open app window, as the operating system describes it.
struct AppWindow {
    /// The native id — CGWindowID (macOS), HWND (Windows), X11 Window (Linux) — which is the
    /// same id Qt keeps inside each QCapturableWindow, so the two lists can be matched.
    quintptr id = 0;
    QString appName;  // "Safari", "Visual Studio Code"
    QString title;    // the window's title (may be empty)
    QString label;    // what the picker shows: "Safari — GitHub"
};

/// The real app windows open right now (core/WindowFilter decides what counts), by id.
///
/// Why not just use QWindowCapture::capturableWindows(): Qt returns every window the OS
/// knows — on macOS that includes the menu bar, the clock, battery and other menu-bar icons,
/// the Dock and invisible helpers — with nothing but a title to tell them apart. The OS can
/// tell us much more (which app owns it, what level it sits on, whether it's a tool window).
///
/// `supported` is set to false where no such information is available (no X11 display on
/// Linux, other platforms); callers then fall back to Qt's list.
/// Implemented per platform: AppWindows_mac.mm, AppWindows_win.cpp, AppWindows_x11.cpp,
/// AppWindows_other.cpp.
QHash<quintptr, AppWindow> listAppWindows(bool *supported = nullptr);

/// Where a window is right now, in Qt's global desktop coordinates (the ones QWindow::x/y
/// use), so a ScreenCam window can be placed around it. Empty if it's closed, minimized or
/// unknown. Cheap enough to call a few times per second.
QRect appWindowBounds(quintptr id);
