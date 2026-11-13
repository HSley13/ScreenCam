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

/// Cameras, microphones and screens for QML (singleton `Devices`), as { id, name } lists for
/// ComboBoxes, and lookups from a stored id back to the device. The microphone list starts
/// with "No microphone" (id "").
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
    static DeviceCatalog *instance();
    static DeviceCatalog *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QVariantList cameras() const;

    /// SCREENCAM_DEMO_CAMERA=<image>: a stand-in camera showing that picture.
    static QString demoCameraImage();
    static constexpr const char *kDemoCameraId = "demo";
    QVariantList microphones() const;
    QVariantList screens() const;
    bool cameraAllowed() const;
    bool microphoneAllowed() const;

    /// Camera, then microphone (only macOS really asks). permissionsChanged() when done.
    Q_INVOKABLE void requestPermissions();

    static QString idOf(const QCameraDevice &device);
    static QString idOf(const QAudioDevice &device);
    static QString idOf(const QScreen *screen);
    static std::optional<QCameraDevice> findCamera(const QString &id);
    static std::optional<QAudioDevice> findMicrophone(const QString &id);
    /// Falls back to the primary screen.
    static QScreen *findScreen(const QString &id);
    /// Logical size x device pixel ratio.
    static QSize screenPixelSize(const QScreen *screen);

signals:
    void camerasChanged();
    void microphonesChanged();
    void screensChanged();
    void permissionsChanged();

private:
    explicit DeviceCatalog(QObject *parent = nullptr);
    void watchScreen(QScreen *screen);

    QMediaDevices m_mediaDevices;  // for its "devices changed" signals
};
