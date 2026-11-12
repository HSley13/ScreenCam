#pragma once

#include <QAudioDevice>
#include <QCameraDevice>
#include <QMediaDevices>
#include <QObject>
#include <QSize>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <optional>

class QJSEngine;
class QQmlEngine;
class QScreen;

/// Exposes the machine's cameras, microphones and screens to QML (as the `Devices` singleton)
/// and offers C++ lookups from a stored id back to the real device.
///
/// Every list is a QVariantList of { id, name } maps so a QML ComboBox can use
/// `textRole: "name"` and `valueRole: "id"`. The camera and microphone lists start with a
/// "None" entry whose id is the empty string.
class DeviceCatalog : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Devices)
    QML_SINGLETON

    Q_PROPERTY(QVariantList cameras READ cameras NOTIFY camerasChanged)
    Q_PROPERTY(QVariantList microphones READ microphones NOTIFY microphonesChanged)
    Q_PROPERTY(QVariantList screens READ screens NOTIFY screensChanged)
    Q_PROPERTY(bool cameraAllowed READ cameraAllowed NOTIFY permissionsChanged)
    Q_PROPERTY(bool microphoneAllowed READ microphoneAllowed NOTIFY permissionsChanged)

public:
    /// One shared instance, used by both QML and the C++ recording code.
    static DeviceCatalog *instance();
    /// Called by the QML engine for the `Devices` singleton.
    static DeviceCatalog *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QVariantList cameras() const;
    QVariantList microphones() const;
    QVariantList screens() const;
    bool cameraAllowed() const;
    bool microphoneAllowed() const;

    /// Asks the OS for camera, then microphone access (only macOS really asks; elsewhere
    /// this resolves immediately). Emits permissionsChanged when done.
    Q_INVOKABLE void requestPermissions();

    // Lookups from the ids stored in the settings.
    static QString idOf(const QCameraDevice &device);
    static QString idOf(const QAudioDevice &device);
    static QString idOf(const QScreen *screen);
    static std::optional<QCameraDevice> findCamera(const QString &id);
    static std::optional<QAudioDevice> findMicrophone(const QString &id);
    /// The screen with that id, or the primary screen if it is gone.
    static QScreen *findScreen(const QString &id);
    /// Real pixel size (logical size x device pixel ratio, e.g. 1728x1117 @2x -> 3456x2234).
    static QSize screenPixelSize(const QScreen *screen);

signals:
    void camerasChanged();
    void microphonesChanged();
    void screensChanged();
    void permissionsChanged();

private:
    explicit DeviceCatalog(QObject *parent = nullptr);
    void watchScreen(QScreen *screen);

    QMediaDevices m_mediaDevices;  // only needed for its "devices changed" signals
};
