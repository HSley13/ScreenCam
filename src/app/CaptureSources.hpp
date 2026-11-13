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

#include "app/ExcludingScreenCapture.hpp"
#include "media/FrameCompositor.hpp"

#include <QElapsedTimer>
#include <QSize>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWindowCapture>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QJSEngine;
class QQmlEngine;

/// The live inputs (QML singleton `Capture`): what is recorded (a screen, a region or a
/// window; "screen" frames below) and the camera. Frames go to the QML previews and to the
/// recorder. Inputs only run while previewed or recording, so the camera light stays off
/// otherwise.
class CaptureSources : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Capture)
    QML_SINGLETON

    Q_PROPERTY(bool previewEnabled READ previewEnabled WRITE setPreviewEnabled NOTIFY previewEnabledChanged)
    // Keeps the camera (only) running for the floating bubble.
    Q_PROPERTY(bool bubbleVisible READ bubbleVisible WRITE setBubbleVisible NOTIFY bubbleVisibleChanged)
    Q_PROPERTY(bool screenActive READ screenActive NOTIFY screenActiveChanged)
    Q_PROPERTY(bool cameraActive READ cameraActive NOTIFY cameraActiveChanged)
    Q_PROPERTY(QString screenError READ screenError NOTIFY screenErrorChanged)
    Q_PROPERTY(QString cameraError READ cameraError NOTIFY cameraErrorChanged)
    // Non-empty when Qt lacks the FFmpeg backend: screen capture and recording can't work.
    Q_PROPERTY(QString backendProblem READ backendProblem CONSTANT)
    Q_PROPERTY(QSize screenSize READ screenSize NOTIFY outputSizeChanged)
    Q_PROPERTY(QSize cameraResolution READ cameraResolution NOTIFY cameraResolutionChanged)
    Q_PROPERTY(QSize outputSize READ outputSize NOTIFY outputSizeChanged)

    Q_PROPERTY(QRectF sourceCrop READ sourceCrop NOTIFY outputSizeChanged)
    // [{ id: <index>, name: "App — Title" }], sorted by name
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(int selectedWindow READ selectedWindow WRITE setSelectedWindow NOTIFY windowsChanged)
    // "Safari — GitHub", "Entire screen · Built-in Display"
    Q_PROPERTY(QString targetLabel READ targetLabel NOTIFY targetChanged)

public:
    static CaptureSources *instance();
    static CaptureSources *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool previewEnabled() const { return m_previewEnabled; }
    void setPreviewEnabled(bool enabled);
    bool bubbleVisible() const { return m_bubbleVisible; }
    void setBubbleVisible(bool visible);
    void setRecordingActive(bool active);

    bool screenActive() const {
        return m_screenCapture.isActive() || m_excludingCapture.isActive() || m_windowCapture.isActive();
    }
    bool cameraActive() const { return (m_camera != nullptr && m_camera->isActive()) || m_demoCamera.isActive(); }
    QString screenError() const { return m_screenError; }
    QString cameraError() const { return m_cameraError; }
    QString backendProblem() const { return m_backendProblem; }
    QSize cameraResolution() const { return m_cameraResolution; }
    /// Real pixel size of what is recorded.
    QSize screenSize() const;
    QRectF sourceCrop() const;
    QVariantList windows() const;
    int selectedWindow() const { return m_selectedWindow; }
    void setSelectedWindow(int index);
    Q_INVOKABLE void refreshWindows();
    QString targetLabel() const;
    /// On the desktop, for the tag that names it. A screen's usable area (so the tag isn't
    /// under the menu bar or Dock); empty for a minimized or closed window.
    Q_INVOKABLE QRect targetBounds() const;
    /// When the capture already shows the pointer, only the highlight halo is added.
    PointerOverlay pointerOverlay() const;
    /// A recording starts with these: ScreenCaptureKit only sends a picture when something
    /// changes, so a still screen would otherwise give it nothing to show.
    QVideoFrame lastScreenFrame() const { return m_lastScreenFrame; }
    QVideoFrame lastCameraFrame() const { return m_lastCameraFrame; }
    /// "Show" buttons: briefly name the target on the desktop (TargetTag).
    Q_INVOKABLE void identifyTarget() { emit identifyRequested(); }
    QSize outputSize() const;

    Q_INVOKABLE void addScreenPreview(QVideoSink *sink);
    Q_INVOKABLE void removeScreenPreview(QVideoSink *sink);
    Q_INVOKABLE void addCameraPreview(QVideoSink *sink);
    Q_INVOKABLE void removeCameraPreview(QVideoSink *sink);

signals:
    void previewEnabledChanged();
    void bubbleVisibleChanged();
    void screenActiveChanged();
    void cameraActiveChanged();
    void screenErrorChanged();
    void cameraErrorChanged();
    void cameraResolutionChanged();
    void outputSizeChanged();
    void windowsChanged();
    void targetChanged();
    void identifyRequested();
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
    bool captureIncludesPointer() const;
    QRect pointerTargetBounds() const;
    void setCameraError(const QString &error);

    QMediaCaptureSession m_screenSession;
    QScreenCapture m_screenCapture;
    // Where available (macOS), instead of m_screenCapture: leaves ScreenCam's own windows out.
    ExcludingScreenCapture m_excludingCapture;
    const bool m_useExcludingCapture = ExcludingScreenCapture::isSupported();
    QVideoSink m_screenSink;
    QList<QPointer<QVideoSink>> m_screenPreviews;
    QString m_screenError;

    QMediaCaptureSession m_windowSession;
    QWindowCapture m_windowCapture;
    QVideoSink m_windowSink;
    QList<QCapturableWindow> m_windows;
    QStringList m_windowLabels;   // parallel to m_windows
    QList<quintptr> m_windowIds;  // keeps the selection across refreshes
    int m_selectedWindow = -1;
    QSize m_windowFrameSize;
    // A window's place, re-read at most every 150 ms (the pointer is sampled ~60x/s).
    mutable QRect m_pointerWindowBounds;
    mutable QElapsedTimer m_pointerWindowBoundsAge;
    QTimer m_screenWatchdog;  // no frame from an active screen capture
    bool m_screenFrameSeen = false;

    QMediaCaptureSession m_cameraSession;
    std::unique_ptr<QCamera> m_camera;
    QString m_cameraDeviceId;
    QVideoSink m_cameraSink;
    QList<QPointer<QVideoSink>> m_cameraPreviews;
    QString m_cameraError;
    QSize m_cameraResolution;
    QVideoFrame m_lastScreenFrame;
    QVideoFrame m_lastCameraFrame;
    QTimer m_demoCamera;  // SCREENCAM_DEMO_CAMERA

    QString m_backendProblem;
    bool m_previewEnabled = false;
    bool m_bubbleVisible = false;
    bool m_recordingActive = false;
};
