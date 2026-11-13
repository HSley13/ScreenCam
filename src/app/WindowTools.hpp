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

    Q_PROPERTY(bool canExcludeFromCapture READ canExcludeFromCapture CONSTANT)

public:
    static WindowTools *instance();
    static WindowTools *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool canExcludeFromCapture() const;

    /// Keeps `window` out of screen recordings: NSWindowSharingNone on macOS,
    /// WDA_EXCLUDEFROMCAPTURE on Windows 10 2004+, false on Linux. Call once it's shown.
    Q_INVOKABLE bool excludeFromCapture(QWindow *window);

private:
    explicit WindowTools(QObject *parent = nullptr);
};
