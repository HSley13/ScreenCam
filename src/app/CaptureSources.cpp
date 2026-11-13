#include "app/CaptureSources.hpp"

#include "app/AppWindows.hpp"
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

#include <algorithm>

#ifdef SCREENCAM_HAS_WINDOW_IDS
#include <QtMultimedia/private/qcapturablewindow_p.h>
#endif

namespace core = screencam::core;

namespace {

constexpr int kScreenWatchdogMs = 4000;

/// The OS's id of a window Qt can capture (CGWindowID / HWND / X11 Window), or 0 if this Qt
/// doesn't let us see it. Qt keeps it in the private part of QCapturableWindow.
quintptr nativeWindowId(const QCapturableWindow &window) {
#ifdef SCREENCAM_HAS_WINDOW_IDS
    const QCapturableWindowPrivate *data = QCapturableWindowPrivate::handle(window);
    return data != nullptr ? static_cast<quintptr>(data->id) : 0;
#else
    Q_UNUSED(window);
    return 0;
#endif
}

QString noScreenPictureMessage() {
    return QCoreApplication::translate(
        "CaptureSources",
        "The screen capture is running but delivers no picture. On macOS, allow ScreenCam under System Settings ▸ "
        "Privacy & Security ▸ Screen & System Audio Recording, then quit and reopen it.");
}

/// Qt's macOS backend reports a missing grant as just "Permissions denied". macOS only
/// re-checks the grant when the app starts, and judges the app that *launched* it: started
/// from a terminal, it's the terminal's permission that counts.
QString screenPermissionMessage() {
    return QCoreApplication::translate(
        "CaptureSources",
        "macOS hasn't given ScreenCam access to the screen. Turn ScreenCam on under System Settings ▸ Privacy & "
        "Security ▸ Screen & System Audio Recording, then quit ScreenCam (⌘Q) and open it again with "
        "`open build/ScreenCam.app` — launched from a terminal, macOS checks the terminal's permission instead. "
        "If it keeps asking after every rebuild, run scripts/create-macos-signing-identity.sh once (see README).");
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
    m_windowSession.setWindowCapture(&m_windowCapture);
    m_windowSession.setVideoSink(&m_windowSink);
    connect(&m_screenSink, &QVideoSink::videoFrameChanged, this, &CaptureSources::onTargetFrame);
    connect(&m_windowSink, &QVideoSink::videoFrameChanged, this, &CaptureSources::onTargetFrame);
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
    connect(&m_excludingCapture, &ExcludingScreenCapture::frameArrived, this, &CaptureSources::onTargetFrame);
    connect(&m_excludingCapture, &ExcludingScreenCapture::activeChanged, this, &CaptureSources::screenActiveChanged);
    connect(&m_excludingCapture, &ExcludingScreenCapture::errorOccurred, this,
            [this](const QString &message, bool permission) {
                if (permission) {
                    setScreenError(screenPermissionMessage());
                } else {
                    setScreenError(message.isEmpty() ? tr("Screen capture failed.") : message);
                }
            });
    connect(&m_windowCapture, &QWindowCapture::activeChanged, this, &CaptureSources::screenActiveChanged);
    connect(&m_windowCapture, &QWindowCapture::errorOccurred, this,
            [this](QWindowCapture::Error, const QString &message) {
                if (message.contains(QStringLiteral("ermission"))) {
                    setScreenError(screenPermissionMessage());
                } else {
                    setScreenError(message.isEmpty() ? tr("Window capture failed.") : message);
                }
            });

    // Without permission (macOS) some backends start "successfully" but never deliver a
    // frame; say so instead of showing an empty preview forever.
    m_screenWatchdog.setSingleShot(true);
    m_screenWatchdog.setInterval(kScreenWatchdogMs);
    connect(&m_screenWatchdog, &QTimer::timeout, this, [this] {
        if ((m_screenCapture.isActive() || m_excludingCapture.isActive()) && !m_screenFrameSeen &&
            m_screenError.isEmpty()) {
            setScreenError(noScreenPictureMessage());
        }
    });
    connect(&m_screenCapture, &QScreenCapture::errorOccurred, this,
            [this](QScreenCapture::Error, const QString &message) {
                if (message.contains(QStringLiteral("ermission"))) {
                    setScreenError(screenPermissionMessage());
                } else {
                    setScreenError(message.isEmpty() ? tr("Screen capture failed.") : message);
                }
            });

    // React to the user's choices.
    RecordingSettings *settings = RecordingSettings::instance();
    connect(settings, &RecordingSettings::modeChanged, this, [this] {
        updateScreen();
        updateCamera();
        emit outputSizeChanged();
    });
    connect(settings, &RecordingSettings::screenIdChanged, this, [this] {
        updateScreen();
        emit outputSizeChanged();
    });
    connect(settings, &RecordingSettings::captureTargetChanged, this, [this] {
        if (RecordingSettings::instance()->captureTarget() == RecordingSettings::CaptureTarget::Window &&
            m_windows.isEmpty()) {
            refreshWindows();
        }
        updateScreen();
        emit outputSizeChanged();
    });
    connect(settings, &RecordingSettings::regionChanged, this, [this] {
        updateScreen();
        emit outputSizeChanged();
    });
    connect(settings, &RecordingSettings::cameraIdChanged, this, &CaptureSources::updateCamera);
    connect(settings, &RecordingSettings::resolutionChanged, this, &CaptureSources::outputSizeChanged);
    m_excludingCapture.setFramesPerSecond(settings->frameRate());
    connect(settings, &RecordingSettings::frameRateChanged, this, [this] {
        m_excludingCapture.setFramesPerSecond(RecordingSettings::instance()->frameRate());
    });
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

void CaptureSources::setBubbleVisible(bool visible) {
    if (m_bubbleVisible == visible) {
        return;
    }
    m_bubbleVisible = visible;
    emit bubbleVisibleChanged();
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

void CaptureSources::onTargetFrame(const QVideoFrame &frame) {
    if (!m_screenFrameSeen) {
        m_screenFrameSeen = true;
        m_screenWatchdog.stop();
        if (m_screenError == noScreenPictureMessage()) {
            setScreenError(QString());
        }
    }
    // A window's frames define its size (and it can be resized while open).
    if (m_windowCapture.isActive() && frame.isValid() && frame.size() != m_windowFrameSize) {
        m_windowFrameSize = frame.size();
        emit outputSizeChanged();
    }
    forward(m_screenPreviews, frame);
    emit screenFrameArrived(frame);
}

void CaptureSources::updateScreen() {
    const RecordingSettings *settings = RecordingSettings::instance();
    const RecordingSettings::CaptureTarget target = settings->captureTarget();
    QScreen *screen = DeviceCatalog::findScreen(settings->screenId());
    if (m_useExcludingCapture) {
        if (m_excludingCapture.screen() != screen) {
            m_excludingCapture.setScreen(screen);
            emit outputSizeChanged();
        }
    } else if (m_screenCapture.screen() != screen) {
        m_screenCapture.setScreen(screen);
        emit outputSizeChanged();
    }
    if (!m_backendProblem.isEmpty()) {
        // The native backends have no screen capture: don't pretend, explain.
        setScreenError(m_backendProblem);
        m_screenCapture.setActive(false);
        m_excludingCapture.setActive(false);
        m_windowCapture.setActive(false);
        return;
    }

    const bool live = settings->usesScreen() && wantsInputs();
    const bool windowChosen = m_selectedWindow >= 0 && m_selectedWindow < m_windows.size();
    const bool wantWindow = live && target == RecordingSettings::CaptureTarget::Window && windowChosen;
    const bool wantScreen = live && target != RecordingSettings::CaptureTarget::Window && screen != nullptr;

    if (live && target == RecordingSettings::CaptureTarget::Window && !windowChosen) {
        setScreenError(tr("Choose a window to record."));
    } else if (live && target == RecordingSettings::CaptureTarget::Region && sourceCrop().isEmpty()) {
        setScreenError(tr("Select the area of the screen to record."));
    } else if ((wantScreen || wantWindow) && !screenActive()) {
        setScreenError(QString());
        m_screenFrameSeen = false;
        m_screenWatchdog.start();
    } else if (m_screenError == tr("Choose a window to record.") ||
               m_screenError == tr("Select the area of the screen to record.")) {
        setScreenError(QString());
    }
    if (!wantScreen && !wantWindow) {
        m_screenWatchdog.stop();
    }

    if (wantWindow) {
        const QCapturableWindow &window = m_windows.at(m_selectedWindow);
        if (m_windowCapture.window() != window) {
            m_windowCapture.setWindow(window);
            m_windowFrameSize = QSize();
            emit outputSizeChanged();
        }
    }
    m_screenCapture.setActive(wantScreen && !m_useExcludingCapture);
    m_excludingCapture.setActive(wantScreen && m_useExcludingCapture);
    m_windowCapture.setActive(wantWindow);
    emit targetChanged();  // every choice that changes what's recorded ends up here
}

QVariantList CaptureSources::windows() const {
    QVariantList list;
    for (int i = 0; i < m_windows.size(); i++) {
        list.append(QVariantMap{{QStringLiteral("id"), i}, {QStringLiteral("name"), m_windowLabels.at(i)}});
    }
    return list;
}

void CaptureSources::refreshWindows() {
    const bool hadSelection = m_selectedWindow >= 0 && m_selectedWindow < m_windows.size();
    const quintptr previousId = hadSelection ? m_windowIds.at(m_selectedWindow) : 0;
    const QString previousLabel = hadSelection ? m_windowLabels.at(m_selectedWindow) : QString();

    // Qt's list = everything the OS calls a window (menu bar, clock, battery ...). Keep the
    // ones the OS's own list says are real app windows; match the two by native id.
    bool detailsAvailable = false;
    QHash<quintptr, AppWindow> appWindows;
#ifdef SCREENCAM_HAS_WINDOW_IDS
    appWindows = listAppWindows(&detailsAvailable);
#endif

    struct Entry {
        QCapturableWindow window;
        quintptr id = 0;
        QString label;
    };
    QList<Entry> entries;
    for (const QCapturableWindow &window : QWindowCapture::capturableWindows()) {
        if (!window.isValid()) {
            continue;
        }
        const quintptr id = nativeWindowId(window);
        if (detailsAvailable) {
            const auto found = appWindows.constFind(id);
            if (found == appWindows.constEnd()) {
                continue;  // not an app window
            }
            entries.append({window, id, found->label});
        } else {
            // Fallback without OS details: at least drop untitled windows and our own.
            const QString title = window.description();
            if (title.isEmpty() || title.startsWith(QCoreApplication::applicationName())) {
                continue;
            }
            entries.append({window, id, title});
        }
    }
    // Alphabetical, so all of an app's windows sit together ("Safari — A", "Safari — B").
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return QString::localeAwareCompare(a.label, b.label) < 0;
    });

    m_windows.clear();
    m_windowLabels.clear();
    m_windowIds.clear();
    m_selectedWindow = -1;
    for (const Entry &entry : entries) {
        // Keep the choice: same window id, or (without ids) the same label.
        const bool same = previousId != 0 ? entry.id == previousId
                                          : (!previousLabel.isEmpty() && entry.label == previousLabel);
        if (same && m_selectedWindow < 0) {
            m_selectedWindow = static_cast<int>(m_windows.size());
        }
        m_windows.append(entry.window);
        m_windowLabels.append(entry.label);
        m_windowIds.append(entry.id);
    }
    emit windowsChanged();
    updateScreen();
}

QString CaptureSources::targetLabel() const {
    const RecordingSettings *settings = RecordingSettings::instance();
    if (!settings->usesScreen()) {
        return tr("Camera only");
    }
    const QScreen *screen = DeviceCatalog::findScreen(settings->screenId());
    const QString screenName = screen != nullptr ? screen->name() : tr("no screen");
    switch (settings->captureTarget()) {
    case RecordingSettings::CaptureTarget::Window:
        if (m_selectedWindow >= 0 && m_selectedWindow < m_windowLabels.size()) {
            return m_windowLabels.at(m_selectedWindow);
        }
        return tr("No window chosen");
    case RecordingSettings::CaptureTarget::Region:
        return tr("Region %1 × %2 · %3")
            .arg(settings->region().width())
            .arg(settings->region().height())
            .arg(screenName);
    case RecordingSettings::CaptureTarget::Screen:
        break;
    }
    return tr("Entire screen · %1").arg(screenName);
}

QRect CaptureSources::targetBounds() const {
    const RecordingSettings *settings = RecordingSettings::instance();
    if (!settings->usesScreen()) {
        return {};
    }
    switch (settings->captureTarget()) {
    case RecordingSettings::CaptureTarget::Window:
        if (m_selectedWindow >= 0 && m_selectedWindow < m_windowIds.size() && m_windowIds.at(m_selectedWindow) != 0) {
            return appWindowBounds(m_windowIds.at(m_selectedWindow));
        }
        return {};
    case RecordingSettings::CaptureTarget::Region: {
        // The region is stored relative to its screen; the outline needs desktop coordinates.
        const QScreen *screen = DeviceCatalog::findScreen(settings->screenId());
        if (screen == nullptr) {
            return {};
        }
        return settings->region().translated(screen->geometry().topLeft());
    }
    case RecordingSettings::CaptureTarget::Screen: {
        const QScreen *screen = DeviceCatalog::findScreen(settings->screenId());
        return screen != nullptr ? screen->availableGeometry() : QRect();
    }
    }
    return {};
}

void CaptureSources::setSelectedWindow(int index) {
    if (index == m_selectedWindow) {
        return;
    }
    m_selectedWindow = index;
    emit windowsChanged();
    updateScreen();
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
    const bool wanted = settings->usesCamera() && (wantsInputs() || m_bubbleVisible) &&
                        DeviceCatalog::instance()->cameraAllowed();
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
    const RecordingSettings *settings = RecordingSettings::instance();
    switch (settings->captureTarget()) {
    case RecordingSettings::CaptureTarget::Window:
        return m_windowFrameSize;  // known once the first frame arrives
    case RecordingSettings::CaptureTarget::Region: {
        const QScreen *screen = DeviceCatalog::findScreen(settings->screenId());
        const QRectF crop = sourceCrop();
        const QSize full = DeviceCatalog::screenPixelSize(screen);
        return {qRound(crop.width() * full.width()), qRound(crop.height() * full.height())};
    }
    case RecordingSettings::CaptureTarget::Screen:
        break;
    }
    return DeviceCatalog::screenPixelSize(DeviceCatalog::findScreen(settings->screenId()));
}

QRectF CaptureSources::sourceCrop() const {
    const RecordingSettings *settings = RecordingSettings::instance();
    if (settings->captureTarget() != RecordingSettings::CaptureTarget::Region) {
        return {0.0, 0.0, 1.0, 1.0};
    }
    const QScreen *screen = DeviceCatalog::findScreen(settings->screenId());
    if (screen == nullptr) {
        return {};
    }
    // Region is stored in the screen's logical coordinates; normalize and keep it on screen.
    const QSizeF size = screen->geometry().size();
    const QRectF normalized(settings->region().x() / size.width(), settings->region().y() / size.height(),
                            settings->region().width() / size.width(), settings->region().height() / size.height());
    return normalized.intersected(QRectF(0.0, 0.0, 1.0, 1.0));
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
