// Linux (X11): the window manager's client list plus the EWMH hints every desktop sets
// (window type, state, allowed actions). Wayland has no global window list for apps; there
// Qt can't capture windows anyway, and we report "unsupported".
#include "app/AppWindows.hpp"
#include "core/WindowFilter.hpp"

#include <QGuiApplication>
#include <QScreen>

#include <algorithm>
#include <vector>

#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace {

/// Reads a window property of 32-bit items (ATOM, CARDINAL, WINDOW lists).
template <typename T>
std::vector<T> readList(Display *display, Window window, const char *name, Atom type) {
    std::vector<T> items;
    const Atom property = XInternAtom(display, name, True);
    if (property == None) {
        return items;  // no window manager has ever set it
    }
    Atom actualType = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long bytesAfter = 0;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(display, window, property, 0, 4096, False, type, &actualType, &format, &count,
                           &bytesAfter, &data) == Success &&
        data != nullptr) {
        // Xlib hands 32-bit items back as `long`, whatever the platform's long size.
        const long *values = reinterpret_cast<const long *>(data);
        for (unsigned long i = 0; i < count; i++) {
            items.push_back(static_cast<T>(values[i]));
        }
    }
    if (data != nullptr) {
        XFree(data);
    }
    return items;
}

bool contains(Display *display, const std::vector<Atom> &atoms, const char *name) {
    const Atom atom = XInternAtom(display, name, True);
    return atom != None && std::find(atoms.begin(), atoms.end(), atom) != atoms.end();
}

QString windowTitle(Display *display, Window window) {
    // _NET_WM_NAME is UTF-8; WM_NAME is the old Latin-1 fallback.
    const Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
    const Atom netName = XInternAtom(display, "_NET_WM_NAME", False);
    Atom actualType = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long bytesAfter = 0;
    unsigned char *data = nullptr;
    QString title;
    if (XGetWindowProperty(display, window, netName, 0, 1024, False, utf8, &actualType, &format, &count,
                           &bytesAfter, &data) == Success &&
        data != nullptr && count > 0) {
        title = QString::fromUtf8(reinterpret_cast<const char *>(data), static_cast<qsizetype>(count));
    }
    if (data != nullptr) {
        XFree(data);
    }
    if (title.isEmpty()) {
        char *name = nullptr;
        if (XFetchName(display, window, &name) != 0 && name != nullptr) {
            title = QString::fromLatin1(name);
            XFree(name);
        }
    }
    return title;
}

/// "Firefox", "Code" ... from WM_CLASS (the class part, which is the app's name).
QString appName(Display *display, Window window) {
    XClassHint hint{};
    QString name;
    if (XGetClassHint(display, window, &hint) != 0) {
        if (hint.res_class != nullptr) {
            name = QString::fromLocal8Bit(hint.res_class);
            XFree(hint.res_class);
        }
        if (hint.res_name != nullptr) {
            XFree(hint.res_name);
        }
    }
    return name;
}

}  // namespace

QHash<quintptr, AppWindow> listAppWindows(bool *supported) {
    QHash<quintptr, AppWindow> result;
    Display *display = XOpenDisplay(nullptr);
    if (supported != nullptr) {
        *supported = display != nullptr;
    }
    if (display == nullptr) {
        return result;
    }
    const Window root = DefaultRootWindow(display);
    const auto clients = readList<Window>(display, root, "_NET_CLIENT_LIST", XA_WINDOW);
    const auto ownPid = static_cast<unsigned long>(getpid());

    for (const Window window : clients) {
        screencam::core::WindowTraits traits;
        XWindowAttributes attributes{};
        if (XGetWindowAttributes(display, window, &attributes) == 0) {
            continue;  // closed in the meantime
        }
        traits.width = attributes.width;
        traits.height = attributes.height;

        // Panels, docks, the desktop, menus, splash screens, notifications: not apps.
        const auto types = readList<Atom>(display, window, "_NET_WM_WINDOW_TYPE", XA_ATOM);
        const auto states = readList<Atom>(display, window, "_NET_WM_STATE", XA_ATOM);
        traits.toolWindow = contains(display, types, "_NET_WM_WINDOW_TYPE_DOCK") ||
                            contains(display, types, "_NET_WM_WINDOW_TYPE_DESKTOP") ||
                            contains(display, types, "_NET_WM_WINDOW_TYPE_TOOLBAR") ||
                            contains(display, types, "_NET_WM_WINDOW_TYPE_MENU") ||
                            contains(display, types, "_NET_WM_WINDOW_TYPE_UTILITY") ||
                            contains(display, types, "_NET_WM_WINDOW_TYPE_SPLASH") ||
                            contains(display, types, "_NET_WM_WINDOW_TYPE_NOTIFICATION") ||
                            contains(display, states, "_NET_WM_STATE_SKIP_TASKBAR");

        // If the window manager publishes allowed actions, an app window can be minimized
        // or maximized; without the property (simple WMs), assume it can.
        const auto actions = readList<Atom>(display, window, "_NET_WM_ALLOWED_ACTIONS", XA_ATOM);
        traits.hasWindowControls = actions.empty() || contains(display, actions, "_NET_WM_ACTION_MINIMIZE") ||
                                   contains(display, actions, "_NET_WM_ACTION_MAXIMIZE_HORZ");

        const auto pids = readList<unsigned long>(display, window, "_NET_WM_PID", XA_CARDINAL);
        traits.ownedBySelf = !pids.empty() && pids.front() == ownPid;

        if (!screencam::core::isUserAppWindow(traits)) {
            continue;
        }
        AppWindow entry;
        entry.id = static_cast<quintptr>(window);
        entry.appName = appName(display, window);
        entry.title = windowTitle(display, window);
        entry.label = QString::fromStdString(
            screencam::core::windowLabel(entry.appName.toStdString(), entry.title.toStdString()));
        result.insert(entry.id, entry);
    }
    XCloseDisplay(display);
    return result;
}

QRect appWindowBounds(quintptr id) {
    // One connection for the app's lifetime: this is polled several times a second.
    static Display *display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        return {};
    }
    const auto window = static_cast<Window>(id);
    XWindowAttributes attributes{};
    if (XGetWindowAttributes(display, window, &attributes) == 0 || attributes.map_state != IsViewable) {
        return {};
    }
    int x = 0;
    int y = 0;
    Window child = None;
    XTranslateCoordinates(display, window, DefaultRootWindow(display), 0, 0, &x, &y, &child);
    // X11 measures in physical pixels; Qt divides by the scale factor (QT_SCALE_FACTOR etc.).
    const QScreen *screen = QGuiApplication::primaryScreen();
    const qreal ratio = screen != nullptr ? screen->devicePixelRatio() : 1.0;
    return QRect(qRound(x / ratio), qRound(y / ratio), qRound(attributes.width / ratio),
                 qRound(attributes.height / ratio));
}
