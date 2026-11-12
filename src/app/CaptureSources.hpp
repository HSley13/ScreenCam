#pragma once

#include <QCamera>
#include <QList>
#include <QMediaCaptureSession>
#include <QObject>
#include <QPointer>
#include <QScreenCapture>
#include <QSize>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QJSEngine;
class QQmlEngine;

/// Owns the live inputs: the screen capture and the camera (QML singleton `Capture`).
///
/// Each input runs in its own QMediaCaptureSession with a QVideoSink we own; every frame is
///  - forwarded to any number of QML VideoOutputs (the previews), and
///  - re-emitted as screenFrameArrived/cameraFrameArrived for the recorder's compositor.
/// Inputs only run while needed: the preview is visible (`previewEnabled`) or a recording is
/// in progress — so the camera light and screen-capture indicator are off otherwise.
class CaptureSources : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Capture)
    QML_SINGLETON

    Q_PROPERTY(bool previewEnabled READ previewEnabled WRITE setPreviewEnabled NOTIFY previewEnabledChanged)
    Q_PROPERTY(bool screenActive READ screenActive NOTIFY screenActiveChanged)
    Q_PROPERTY(bool cameraActive READ cameraActive NOTIFY cameraActiveChanged)
    Q_PROPERTY(QString screenError READ screenError NOTIFY screenErrorChanged)
    Q_PROPERTY(QString cameraError READ cameraError NOTIFY cameraErrorChanged)
    Q_PROPERTY(QSize screenSize READ screenSize NOTIFY outputSizeChanged)
    Q_PROPERTY(QSize cameraResolution READ cameraResolution NOTIFY cameraResolutionChanged)
    Q_PROPERTY(QSize outputSize READ outputSize NOTIFY outputSizeChanged)

public:
    static CaptureSources *instance();
    static CaptureSources *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool previewEnabled() const { return m_previewEnabled; }
    void setPreviewEnabled(bool enabled);
    /// Keeps the inputs running while recording, even if the window is hidden.
    void setRecordingActive(bool active);

    bool screenActive() const { return m_screenCapture.isActive(); }
    bool cameraActive() const { return m_camera != nullptr && m_camera->isActive(); }
    QString screenError() const { return m_screenError; }
    QString cameraError() const { return m_cameraError; }
    QSize cameraResolution() const { return m_cameraResolution; }
    /// Real pixel size of the selected screen.
    QSize screenSize() const;
    /// Size of the video file for the current settings (see computeOutputSize in the .cpp).
    QSize outputSize() const;

    Q_INVOKABLE void addScreenPreview(QVideoSink *sink);
    Q_INVOKABLE void removeScreenPreview(QVideoSink *sink);
    Q_INVOKABLE void addCameraPreview(QVideoSink *sink);
    Q_INVOKABLE void removeCameraPreview(QVideoSink *sink);

signals:
    void previewEnabledChanged();
    void screenActiveChanged();
    void cameraActiveChanged();
    void screenErrorChanged();
    void cameraErrorChanged();
    void cameraResolutionChanged();
    void outputSizeChanged();
    void screenFrameArrived(const QVideoFrame &frame);
    void cameraFrameArrived(const QVideoFrame &frame);

private:
    explicit CaptureSources(QObject *parent = nullptr);

    void updateScreen();
    void updateCamera();
    void releaseCamera();
    bool wantsInputs() const { return m_previewEnabled || m_recordingActive; }
    void setScreenError(const QString &error);
    void setCameraError(const QString &error);

    // Screen
    QMediaCaptureSession m_screenSession;
    QScreenCapture m_screenCapture;
    QVideoSink m_screenSink;
    QList<QPointer<QVideoSink>> m_screenPreviews;
    QString m_screenError;

    // Camera
    QMediaCaptureSession m_cameraSession;
    std::unique_ptr<QCamera> m_camera;
    QString m_cameraDeviceId;
    QVideoSink m_cameraSink;
    QList<QPointer<QVideoSink>> m_cameraPreviews;
    QString m_cameraError;
    QSize m_cameraResolution;

    bool m_previewEnabled = false;
    bool m_recordingActive = false;
};
