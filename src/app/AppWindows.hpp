#pragma once

#include <QHash>
#include <QRect>
#include <QString>

struct AppWindow {
    /// CGWindowID / HWND / X11 Window: the id inside each QCapturableWindow.
    quintptr id = 0;
    QString appName;
    QString title;  // may be empty
    QString label;  // "Safari — GitHub"
};

/// The real app windows open right now, by id. Qt's capturableWindows() lists every OS window
/// (menu-bar icons, the Dock, ...) with only a title; the OS can tell us which are app windows.
/// `supported` is false where it can't (then fall back to Qt's list).
QHash<quintptr, AppWindow> listAppWindows(bool *supported = nullptr);

/// In Qt's global coordinates; empty if closed, minimized or unknown.
QRect appWindowBounds(quintptr id);
