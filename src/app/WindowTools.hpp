#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;
class QWindow;

/// Native window tweaks that Qt doesn't expose (QML singleton `WindowTools`).
class WindowTools : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    /// True where excludeFromCapture() works (macOS, Windows).
    Q_PROPERTY(bool canExcludeFromCapture READ canExcludeFromCapture CONSTANT)

public:
    static WindowTools *instance();
    static WindowTools *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool canExcludeFromCapture() const;

    /// Keeps `window` out of screen recordings and screenshots — ScreenCam's own windows
    /// (control panel, floating camera bubble, mini control bar) never end up in the video,
    /// and the live preview no longer shows itself in an endless loop.
    ///  - macOS:   NSWindow.sharingType = NSWindowSharingNone
    ///  - Windows: SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)  (Windows 10 2004+)
    ///  - Linux:   no equivalent; returns false.
    /// Call it once the window is shown (it needs the native window to exist).
    Q_INVOKABLE bool excludeFromCapture(QWindow *window);

private:
    explicit WindowTools(QObject *parent = nullptr);
};
