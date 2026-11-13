#include "app/WindowTools.hpp"

#include <QCoreApplication>
#include <QJSEngine>
#include <QWindow>

#if defined(Q_OS_WIN)
#include <windows.h>
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011  // missing from older Windows SDK headers
#endif
#endif

#if defined(Q_OS_MACOS)
bool screencamExcludeNativeWindowFromCapture(WId windowId);  // WindowTools_mac.mm
#endif

WindowTools *WindowTools::instance() {
    static WindowTools *tools = new WindowTools(qApp);
    return tools;
}

WindowTools *WindowTools::create(QQmlEngine *, QJSEngine *) {
    WindowTools *tools = instance();
    QJSEngine::setObjectOwnership(tools, QJSEngine::CppOwnership);
    return tools;
}

WindowTools::WindowTools(QObject *parent)
    : QObject(parent) {
}

bool WindowTools::canExcludeFromCapture() const {
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}

bool WindowTools::excludeFromCapture(QWindow *window) {
    if (window == nullptr) {
        return false;
    }
#if defined(Q_OS_MACOS)
    return screencamExcludeNativeWindowFromCapture(window->winId());
#elif defined(Q_OS_WIN)
    return SetWindowDisplayAffinity(reinterpret_cast<HWND>(window->winId()), WDA_EXCLUDEFROMCAPTURE) != FALSE;
#else
    return false;
#endif
}
