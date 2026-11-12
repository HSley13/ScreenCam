#pragma once

#include <QCamera>
#include <QCapturableWindow>
#include <QList>
#include <QMediaCaptureSession>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QVariantList>
#include <QScreenCapture>
#include <QSize>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWindowCapture>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QJSEngine;
class QQmlEngine;

/// Owns the live inputs: what is recorded (a screen, a region of it, or one window) and the
/// camera (QML singleton `Capture`). "Screen" frames below means frames of that target.
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
    // Non-empty when Qt lacks the FFmpeg backend: screen capture and recording can't work.
    Q_PROPERTY(QString backendProblem READ backendProblem CONSTANT)
    Q_PROPERTY(QSize screenSize READ screenSize NOTIFY outputSizeChanged)
    Q_PROPERTY(QSize cameraResolution READ cameraResolution NOTIFY cameraResolutionChanged)
    Q_PROPERTY(QSize outputSize READ outputSize NOTIFY outputSizeChanged)
    // Normalized part of the screen frame that is recorded (whole frame unless Region).
    Q_PROPERTY(QRectF sourceCrop READ sourceCrop NOTIFY outputSizeChanged)
    // Windows that can be recorded: [{ id: <index>, name: <title> }], see refreshWindows().
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(int selectedWindow READ selectedWindow WRITE setSelectedWindow NOTIFY windowsChanged)

public:
    static CaptureSources *instance();
    static CaptureSources *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool previewEnabled() const { return m_previewEnabled; }
    void setPreviewEnabled(bool enabled);
    /// Keeps the inputs running while recording, even if the window is hidden.
    void setRecordingActive(bool active);

    bool screenActive() const { return m_screenCapture.isActive() || m_windowCapture.isActive(); }
    bool cameraActive() const { return m_camera != nullptr && m_camera->isActive(); }
    QString screenError() const { return m_screenError; }
    QString cameraError() const { return m_cameraError; }
    QString backendProblem() const { return m_backendProblem; }
    QSize cameraResolution() const { return m_cameraResolution; }
    /// Real pixel size of what is recorded: the screen, the region, or the window.
    QSize screenSize() const;
    QRectF sourceCrop() const;
    QVariantList windows() const;
    int selectedWindow() const { return m_selectedWindow; }
    void setSelectedWindow(int index);
    /// Re-reads the list of open windows (windows come and go, so the UI asks on demand).
    Q_INVOKABLE void refreshWindows();
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
    void windowsChanged();
    void screenFrameArrived(const QVideoFrame &frame);
    void cameraFrameArrived(const QVideoFrame &frame);

private:
    explicit CaptureSources(QObject *parent = nullptr);

    void updateScreen();
    void onTargetFrame(const QVideoFrame &frame);
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

    // Window (alternative target, same downstream path as the screen)
    QMediaCaptureSession m_windowSession;
    QWindowCapture m_windowCapture;
    QVideoSink m_windowSink;
    QList<QCapturableWindow> m_windows;
    int m_selectedWindow = -1;
    QSize m_windowFrameSize;
    QTimer m_screenWatchdog;  // fires if an active screen capture delivers no frame
    bool m_screenFrameSeen = false;

    // Camera
    QMediaCaptureSession m_cameraSession;
    std::unique_ptr<QCamera> m_camera;
    QString m_cameraDeviceId;
    QVideoSink m_cameraSink;
    QList<QPointer<QVideoSink>> m_cameraPreviews;
    QString m_cameraError;
    QSize m_cameraResolution;

    QString m_backendProblem;
    bool m_previewEnabled = false;
    bool m_recordingActive = false;
};
