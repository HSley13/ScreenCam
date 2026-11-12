#include "app/CaptureSources.hpp"

#include "app/DeviceCatalog.hpp"
#include "app/RecordingSettings.hpp"
#include "core/CameraLayout.hpp"
#include "core/Resolution.hpp"
#include "media/MediaBackendCheck.hpp"

#include <QCameraDevice>
#include <QCameraFormat>
#include <QCoreApplication>
#include <QJSEngine>
#include <QScreen>

namespace core = screencam::core;

namespace {

constexpr int kScreenWatchdogMs = 4000;

QString noScreenPictureMessage() {
    return QCoreApplication::translate(
        "CaptureSources",
        "The screen capture is running but delivers no picture. On macOS, allow ScreenCam under System Settings ▸ "
        "Privacy & Security ▸ Screen & System Audio Recording, then quit and reopen it.");
}

/// Best camera mode for a bubble: the largest resolution up to 1080p that runs at >= 24 fps
/// (falls back to the largest mode if none is that fast). 4K webcam modes would only cost CPU,
/// since the camera never fills more than a 1080p-sized area of the video.
QCameraFormat pickCameraFormat(const QCameraDevice &device) {
    QCameraFormat best;
    QCameraFormat largest;
    for (const QCameraFormat &format : device.videoFormats()) {
        const QSize size = format.resolution();
        if (largest.isNull() || size.width() * size.height() > largest.resolution().width() * largest.resolution().height()) {
            largest = format;
        }
        if (size.height() > 1080 || format.maxFrameRate() < 24.0f) {
            continue;
        }
        const QSize bestSize = best.isNull() ? QSize() : best.resolution();
        const bool bigger = best.isNull() || size.width() * size.height() > bestSize.width() * bestSize.height();
        const bool sameSizeFaster =
            !best.isNull() && size == bestSize && format.maxFrameRate() > best.maxFrameRate();
        if (bigger || sameSizeFaster) {
            best = format;
        }
    }
    return best.isNull() ? largest : best;
}

void forward(const QList<QPointer<QVideoSink>> &sinks, const QVideoFrame &frame) {
    for (const QPointer<QVideoSink> &sink : sinks) {
        if (sink != nullptr) {
            sink->setVideoFrame(frame);
        }
    }
}

}  // namespace

CaptureSources *CaptureSources::instance() {
    static CaptureSources *sources = new CaptureSources(qApp);
    return sources;
}

CaptureSources *CaptureSources::create(QQmlEngine *, QJSEngine *) {
    CaptureSources *sources = instance();
    QJSEngine::setObjectOwnership(sources, QJSEngine::CppOwnership);
    return sources;
}

CaptureSources::CaptureSources(QObject *parent)
    : QObject(parent)
    , m_backendProblem(MediaBackendCheck::currentProblem()) {
    if (!m_backendProblem.isEmpty()) {
        qWarning().noquote() << "ScreenCam:" << m_backendProblem;
    }
    m_screenSession.setScreenCapture(&m_screenCapture);
    m_screenSession.setVideoSink(&m_screenSink);
    m_cameraSession.setVideoSink(&m_cameraSink);

    // Frames may be produced on a media thread; using `this` as context makes Qt deliver
    // them on the main thread, where the QML previews live.
    connect(&m_screenSink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        if (!m_screenFrameSeen) {
            m_screenFrameSeen = true;
            m_screenWatchdog.stop();
            if (m_screenError == noScreenPictureMessage()) {
                setScreenError(QString());
            }
        }
        forward(m_screenPreviews, frame);
        emit screenFrameArrived(frame);
    });
    connect(&m_cameraSink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        forward(m_cameraPreviews, frame);
        if (frame.size() != m_cameraResolution && frame.isValid()) {
            m_cameraResolution = frame.size();  // trust real frames over the requested format
            emit cameraResolutionChanged();
            emit outputSizeChanged();
        }
        emit cameraFrameArrived(frame);
    });

    connect(&m_screenCapture, &QScreenCapture::activeChanged, this, &CaptureSources::screenActiveChanged);

    // Without permission (macOS) some backends start "successfully" but never deliver a
    // frame; say so instead of showing an empty preview forever.
    m_screenWatchdog.setSingleShot(true);
    m_screenWatchdog.setInterval(kScreenWatchdogMs);
    connect(&m_screenWatchdog, &QTimer::timeout, this, [this] {
        if (m_screenCapture.isActive() && !m_screenFrameSeen && m_screenError.isEmpty()) {
            setScreenError(noScreenPictureMessage());
        }
    });
    connect(&m_screenCapture, &QScreenCapture::errorOccurred, this,
            [this](QScreenCapture::Error, const QString &message) {
                setScreenError(message.isEmpty() ? tr("Screen capture failed.") : message);
            });

    // React to the user's choices.
    RecordingSettings *settings = RecordingSettings::instance();
    connect(settings, &RecordingSettings::modeChanged, this, [this] {
        updateScreen();
        updateCamera();
        emit outputSizeChanged();
    });
    connect(settings, &RecordingSettings::screenIdChanged, this, &CaptureSources::updateScreen);
    connect(settings, &RecordingSettings::cameraIdChanged, this, &CaptureSources::updateCamera);
    connect(settings, &RecordingSettings::resolutionChanged, this, &CaptureSources::outputSizeChanged);
    connect(settings, &RecordingSettings::cameraOnlyOrientationChanged, this, &CaptureSources::outputSizeChanged);

    DeviceCatalog *devices = DeviceCatalog::instance();
    connect(devices, &DeviceCatalog::camerasChanged, this, &CaptureSources::updateCamera);
    connect(devices, &DeviceCatalog::permissionsChanged, this, &CaptureSources::updateCamera);
    connect(devices, &DeviceCatalog::screensChanged, this, [this] {
        updateScreen();
        emit outputSizeChanged();
    });
}

void CaptureSources::setPreviewEnabled(bool enabled) {
    if (m_previewEnabled == enabled) {
        return;
    }
    m_previewEnabled = enabled;
    emit previewEnabledChanged();
    updateScreen();
    updateCamera();
}

void CaptureSources::setRecordingActive(bool active) {
    if (m_recordingActive == active) {
        return;
    }
    m_recordingActive = active;
    updateScreen();
    updateCamera();
}

void CaptureSources::updateScreen() {
    const RecordingSettings *settings = RecordingSettings::instance();
    QScreen *screen = DeviceCatalog::findScreen(settings->screenId());
    if (m_screenCapture.screen() != screen) {
        m_screenCapture.setScreen(screen);
        emit outputSizeChanged();
    }
    const bool wanted = settings->usesScreen() && wantsInputs() && screen != nullptr;
    if (!m_backendProblem.isEmpty()) {
        // The native backends have no screen capture: don't pretend, explain.
        setScreenError(m_backendProblem);
        m_screenCapture.setActive(false);
        return;
    }
    if (wanted && !m_screenCapture.isActive()) {
        setScreenError(QString());
        m_screenFrameSeen = false;
        m_screenWatchdog.start();
    }
    if (!wanted) {
        m_screenWatchdog.stop();
    }
    m_screenCapture.setActive(wanted);
}

void CaptureSources::releaseCamera() {
    if (m_camera == nullptr) {
        return;
    }
    m_cameraSession.setCamera(nullptr);
    m_camera.reset();  // releases the device: the camera light turns off
    m_cameraDeviceId.clear();
    emit cameraActiveChanged();
}

void CaptureSources::updateCamera() {
    const RecordingSettings *settings = RecordingSettings::instance();
    const bool wanted = settings->usesCamera() && wantsInputs() && DeviceCatalog::instance()->cameraAllowed();
    const std::optional<QCameraDevice> device = DeviceCatalog::findCamera(settings->cameraId());

    if (!wanted || !device) {
        releaseCamera();
        if (wanted && !settings->cameraId().isEmpty()) {
            setCameraError(tr("The selected camera is not connected."));
        }
        return;
    }
    if (m_camera != nullptr && m_cameraDeviceId == settings->cameraId()) {
        return;  // already running the right camera
    }

    releaseCamera();
    setCameraError(QString());
    m_camera = std::make_unique<QCamera>(*device);
    m_cameraDeviceId = settings->cameraId();
    const QCameraFormat format = pickCameraFormat(*device);
    if (!format.isNull()) {
        m_camera->setCameraFormat(format);
        m_cameraResolution = format.resolution();
        emit cameraResolutionChanged();
        emit outputSizeChanged();
    }
    connect(m_camera.get(), &QCamera::activeChanged, this, &CaptureSources::cameraActiveChanged);
    connect(m_camera.get(), &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &message) {
        setCameraError(message.isEmpty() ? tr("The camera stopped working.") : message);
    });
    m_cameraSession.setCamera(m_camera.get());
    m_camera->start();
}

QSize CaptureSources::screenSize() const {
    return DeviceCatalog::screenPixelSize(DeviceCatalog::findScreen(RecordingSettings::instance()->screenId()));
}

QSize CaptureSources::outputSize() const {
    const RecordingSettings *settings = RecordingSettings::instance();
    if (settings->mode() == RecordingSettings::CaptureMode::CameraOnly) {
        // Just you: the largest 16:9 or 9:16 rectangle the camera can fill.
        const core::PixelSize size = core::cameraRecordingSize(
            core::PixelSize{m_cameraResolution.width(), m_cameraResolution.height()},
            static_cast<core::CameraOrientation>(settings->cameraOnlyOrientation()));
        return {size.width, size.height};
    }
    // Screen modes: the screen's pixels scaled to the chosen preset (aspect ratio kept).
    const QSize screen = screenSize();
    const core::PixelSize size = core::computeCaptureSize(core::PixelSize{screen.width(), screen.height()},
                                                          static_cast<core::ResolutionPreset>(settings->resolution()));
    return {size.width, size.height};
}

void CaptureSources::addScreenPreview(QVideoSink *sink) {
    if (sink != nullptr && !m_screenPreviews.contains(sink)) {
        m_screenPreviews.append(sink);
    }
}

void CaptureSources::removeScreenPreview(QVideoSink *sink) {
    m_screenPreviews.removeAll(sink);
}

void CaptureSources::addCameraPreview(QVideoSink *sink) {
    if (sink != nullptr && !m_cameraPreviews.contains(sink)) {
        m_cameraPreviews.append(sink);
    }
}

void CaptureSources::removeCameraPreview(QVideoSink *sink) {
    m_cameraPreviews.removeAll(sink);
}

void CaptureSources::setScreenError(const QString &error) {
    if (m_screenError != error) {
        m_screenError = error;
        emit screenErrorChanged();
    }
}

void CaptureSources::setCameraError(const QString &error) {
    if (m_cameraError != error) {
        m_cameraError = error;
        emit cameraErrorChanged();
    }
}
