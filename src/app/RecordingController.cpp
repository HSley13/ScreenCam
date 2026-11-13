#include "app/RecordingController.hpp"

#include "app/BubbleStyle.hpp"
#include "app/CaptureSources.hpp"
#include "app/DeviceCatalog.hpp"
#include "app/RecordingSettings.hpp"
#include "core/EncoderPlan.hpp"
#include "core/Resolution.hpp"
#include "core/TimeFormat.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QScreen>
#include <QUrl>

namespace core = screencam::core;

namespace {
// How long to wait after minimizing the window before the first frame, so the
// minimize animation doesn't end up at the start of the video.
constexpr int kHideDelayMs = 450;
}  // namespace

RecordingController *RecordingController::instance() {
    static RecordingController *controller = new RecordingController(qApp);
    return controller;
}

RecordingController *RecordingController::create(QQmlEngine *, QJSEngine *) {
    RecordingController *controller = instance();
    QJSEngine::setObjectOwnership(controller, QJSEngine::CppOwnership);
    return controller;
}

RecordingController::RecordingController(QObject *parent)
    : QObject(parent) {
    m_countdownTimer.setInterval(1000);
    connect(&m_countdownTimer, &QTimer::timeout, this, [this] {
        m_countdownRemaining--;
        emit countdownRemainingChanged();
        if (m_countdownRemaining <= 0) {
            m_countdownTimer.stop();
            beginRecording();
        }
    });

    // The mouse moves even when nothing else on screen changes (and some captures only
    // deliver a picture on change), so it is sampled on its own clock, not per screen frame.
    m_pointerTicker.setInterval(16);
    connect(&m_pointerTicker, &QTimer::timeout, this, [this] {
        if (m_pipeline.isActive() && !m_pipeline.isPaused()) {
            m_pipeline.pushPointer(CaptureSources::instance()->pointerOverlay());
        }
    });

    m_elapsedTicker.setInterval(250);
    connect(&m_elapsedTicker, &QTimer::timeout, this, &RecordingController::elapsedChanged);

    // Live inputs -> pipelines (only while they record).
    CaptureSources *capture = CaptureSources::instance();
    connect(capture, &CaptureSources::screenFrameArrived, this, [this](const QVideoFrame &frame) {
        if (m_pipeline.isActive()) {
            m_pipeline.pushScreenFrame(frame);
        }
    });
    connect(capture, &CaptureSources::cameraFrameArrived, this, [this](const QVideoFrame &frame) {
        if (m_cameraPipeline.isActive()) {
            m_cameraPipeline.pushCameraFrame(frame);  // camera.mp4, next to screen.mp4
        }
    });

    connect(&m_pipeline, &RecordingPipeline::clockStarted, this, [this](qint64 ms) { m_screenClockMs = ms; });
    connect(&m_cameraPipeline, &RecordingPipeline::clockStarted, this, [this](qint64 ms) { m_cameraClockMs = ms; });
    connect(&m_pipeline, &RecordingPipeline::finished, this,
            [this](const QString &) { onPipelineDone(false, QString()); });
    connect(&m_pipeline, &RecordingPipeline::failed, this,
            [this](const QString &message) { onPipelineDone(false, message); });
    connect(&m_cameraPipeline, &RecordingPipeline::finished, this,
            [this](const QString &) { onPipelineDone(true, QString()); });
    connect(&m_cameraPipeline, &RecordingPipeline::failed, this,
            [this](const QString &message) { onPipelineDone(true, message); });
}

QString RecordingController::elapsedText() const {
    return QString::fromStdString(core::formatElapsed(static_cast<double>(activeMilliseconds()) / 1000.0));
}

QString RecordingController::lastRecordingName() const {
    return QFileInfo(m_lastRecording).fileName();
}

qint64 RecordingController::activeMilliseconds() const {
    if (!m_clock.isValid()) {
        return 0;
    }
    qint64 active = m_clock.elapsed() - m_pausedMs;
    if (m_pauseStartedMs >= 0) {
        active -= m_clock.elapsed() - m_pauseStartedMs;
    }
    return active;
}

QString RecordingController::validate() const {
    const RecordingSettings *settings = RecordingSettings::instance();
    const CaptureSources *capture = CaptureSources::instance();

    // Recording feeds frames through QVideoFrameInput, which needs the FFmpeg backend.
    if (!capture->backendProblem().isEmpty()) {
        return capture->backendProblem();
    }
    if (!capture->screenError().isEmpty()) {
        return capture->screenError();
    }
    // Every recording is screen + you: without a working camera there's nothing to make.
    if (!DeviceCatalog::instance()->cameraAllowed()) {
        return tr("ScreenCam isn't allowed to use the camera. Allow it in your system's privacy settings.");
    }
    if (settings->cameraId().isEmpty()) {
        return tr("Choose a camera in the sidebar.");
    }
    if (!capture->cameraActive() || capture->cameraResolution().isEmpty()) {
        return capture->cameraError().isEmpty() ? tr("The camera isn't ready yet.") : capture->cameraError();
    }
    if (capture->outputSize().isEmpty()) {
        return tr("Nothing to record: no screen or camera picture is available.");
    }
    if (!QDir().mkpath(settings->outputFolder())) {
        return tr("Can't write to %1.").arg(settings->outputFolder());
    }
    return {};
}

void RecordingController::start() {
    if (m_state != State::Idle) {
        return;
    }
    clearError();
    const QString problem = validate();
    if (!problem.isEmpty()) {
        setError(problem);
        return;
    }

    // Keep the camera/screen running even when the window gets minimized.
    CaptureSources::instance()->setRecordingActive(true);
    // Get out of the way right away: the countdown runs in the floating mini control bar.
    if (RecordingSettings::instance()->hideWindowWhileRecording()) {
        emit windowHideRequested();
    }

    const int countdown = RecordingSettings::instance()->countdownSeconds();
    if (countdown > 0) {
        m_countdownRemaining = countdown;
        emit countdownRemainingChanged();
        setState(State::Countdown);
        m_countdownTimer.start();
    } else {
        beginRecording();
    }
}

void RecordingController::beginRecording() {
    const RecordingSettings *settings = RecordingSettings::instance();
    // Without a countdown the window was hidden a moment ago: let the animation finish.
    const bool justHidden = settings->hideWindowWhileRecording() && settings->countdownSeconds() == 0;
    setState(State::Recording);

    QTimer::singleShot(justHidden ? kHideDelayMs : 0, this, [this] {
        if (m_state == State::Recording && !m_pipeline.isActive()) {
            startPipelines();
        }
    });
}

void RecordingController::startPipelines() {
    const RecordingSettings *settings = RecordingSettings::instance();
    const CaptureSources *capture = CaptureSources::instance();
    const int fps = settings->frameRate();
    auto bitRateFor = [fps](const QSize &size) {
        return core::planEncoder(core::PixelSize{size.width(), size.height()}, fps).averageBitRate;
    };

    // Every recording is an editable project: the screen and the camera in two separate files,
    // so you can still move yourself around afterwards.
    RecordingPipeline::Request request;
    request.compositor.canvasSize = capture->outputSize();
    request.compositor.sourceCrop = capture->sourceCrop();
    request.compositor.framesPerSecond = fps;
    request.compositor.bubble = BubbleStyle::instance()->snapshot();
    request.compositor.layout = CompositeLayout::ScreenOnly;  // the camera is added at export
    if (DeviceCatalog::instance()->microphoneAllowed()) {
        request.microphone = DeviceCatalog::findMicrophone(settings->microphoneId());
    }
    request.videoBitRate = bitRateFor(request.compositor.canvasSize);

    m_project = Project{};
    m_project.folder = nextProjectFolder();
    if (!QDir().mkpath(m_project.folder)) {
        setError(tr("Can't create %1.").arg(m_project.folder));
        finish();
        return;
    }
    m_project.canvasSize = request.compositor.canvasSize;
    m_project.framesPerSecond = fps;
    m_project.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    // First segment: the current bubble look, placed where the floating bubble was.
    ProjectSegment first;
    first.style = BubbleStyle::instance()->snapshot();
    first.style.center = bubbleCenterInTarget();
    m_project.segments = {first};
    request.outputPath = m_project.screenPath();

    m_pendingPipelines = 0;
    m_screenSaved = false;
    m_cameraSaved = false;
    m_screenClockMs = -1;
    m_cameraClockMs = -1;
    m_clock.start();
    m_pausedMs = 0;
    m_pauseStartedMs = -1;
    m_elapsedTicker.start();
    emit elapsedChanged();

    m_startingPipelines = true;
    const bool screenStarted = m_pipeline.start(request);
    if (screenStarted) {
        m_pendingPipelines++;
        if (capture->lastScreenFrame().isValid()) {
            m_pipeline.pushScreenFrame(capture->lastScreenFrame());  // what's on screen right now
        }
    }
    if (screenStarted) {
        // The camera's full frame, unmirrored: mirroring and cropping happen at export time.
        const QSize camera = capture->cameraResolution();
        RecordingPipeline::Request cameraRequest;
        cameraRequest.compositor.canvasSize = QSize(core::makeEven(camera.width()), core::makeEven(camera.height()));
        cameraRequest.compositor.layout = CompositeLayout::CameraOnly;
        cameraRequest.compositor.framesPerSecond = fps;
        cameraRequest.compositor.bubble.mirrored = false;
        cameraRequest.videoBitRate = bitRateFor(cameraRequest.compositor.canvasSize);
        cameraRequest.outputPath = m_project.cameraPath();
        if (m_cameraPipeline.start(cameraRequest)) {
            m_pendingPipelines++;
            if (capture->lastCameraFrame().isValid()) {
                m_cameraPipeline.pushCameraFrame(capture->lastCameraFrame());
            }
        } else {
            m_project.cameraFile.clear();
        }
    }
    m_startingPipelines = false;

    if (!screenStarted) {
        setError(tr("The recording could not start."));
        finish();
    }
}

void RecordingController::onPipelineDone(bool camera, const QString &error) {
    if (m_startingPipelines || m_state == State::Idle) {
        return;  // a request was rejected inside start(): handled by startPipelines()
    }
    if (!error.isEmpty()) {
        setError(error);
    }
    if (camera) {
        m_cameraSaved = error.isEmpty();
    } else {
        m_screenSaved = error.isEmpty();
    }
    if (m_pendingPipelines > 0) {
        m_pendingPipelines--;
    }
    if (m_pendingPipelines > 0) {
        return;  // wait for the other file
    }

    completeProject();
    const bool saved = m_lastRecording == m_project.folder && !m_project.folder.isEmpty();
    finish();  // brings the recorder window back...
    if (saved) {
        emit openProjectRequested(m_project.folder);  // ...then the editor opens on top of it
    }
}

void RecordingController::completeProject() {
    if (!m_screenSaved) {
        return;  // nothing usable was written
    }
    if (!m_cameraSaved) {
        m_project.cameraFile.clear();
    }
    m_project.durationMs = m_recordedMs;
    if (m_screenClockMs >= 0 && m_cameraClockMs >= 0) {
        m_project.cameraOffsetMs = m_cameraClockMs - m_screenClockMs;
    }
    QString error;
    if (!m_project.save(&error)) {
        setError(tr("Couldn't save the project: %1").arg(error));
        return;
    }
    m_lastRecording = m_project.folder;
    emit lastRecordingChanged();
}

void RecordingController::stop() {
    switch (m_state) {
    case State::Countdown:
        cancelCountdown();
        break;
    case State::Recording:
    case State::Paused:
        if (!m_pipeline.isActive()) {
            finish();  // stopped during the hide delay: nothing was written yet
            return;
        }
        m_recordedMs = activeMilliseconds();
        m_elapsedTicker.stop();
        setState(State::Finalizing);
        m_pipeline.stop();
        m_cameraPipeline.stop();  // no-op unless recording a project
        break;
    case State::Idle:
    case State::Finalizing:
        break;
    }
}

void RecordingController::togglePause() {
    if (m_state == State::Recording && m_pipeline.isActive()) {
        m_pipeline.pause();
        m_cameraPipeline.pause();
        m_pauseStartedMs = m_clock.elapsed();
        setState(State::Paused);
    } else if (m_state == State::Paused) {
        m_pipeline.resume();
        m_cameraPipeline.resume();
        m_pausedMs += m_clock.elapsed() - m_pauseStartedMs;
        m_pauseStartedMs = -1;
        setState(State::Recording);
    }
    emit elapsedChanged();
}

void RecordingController::toggleRecording() {
    if (m_state == State::Idle) {
        start();
    } else {
        stop();
    }
}

void RecordingController::cancelCountdown() {
    if (m_state != State::Countdown) {
        return;
    }
    m_countdownTimer.stop();
    m_countdownRemaining = 0;
    emit countdownRemainingChanged();
    CaptureSources::instance()->setRecordingActive(false);
    setState(State::Idle);
}

void RecordingController::finish() {
    if (m_state == State::Idle) {
        return;  // already finished (e.g. failed() fired synchronously inside start())
    }
    m_countdownTimer.stop();
    m_elapsedTicker.stop();
    CaptureSources::instance()->setRecordingActive(false);
    setState(State::Idle);
    emit windowShowRequested();
}

void RecordingController::openLastRecording() {
    if (m_lastRecording.isEmpty()) {
        return;
    }
    if (QFileInfo(m_lastRecording).isDir()) {
        emit openProjectRequested(m_lastRecording);  // a project: open it in the editor
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastRecording));  // a finished video: play it
    }
}

void RecordingController::setBubbleDesktopCenter(const QPointF &center) {
    m_bubbleDesktopCenter = center;
}

core::PointD RecordingController::bubbleCenterInTarget() const {
    const core::PointD fallback{0.12, 0.84};  // bottom-left, like the floating bubble's start
    const RecordingSettings *settings = RecordingSettings::instance();
    const QScreen *screen = DeviceCatalog::findScreen(settings->screenId());
    if (screen == nullptr || m_bubbleDesktopCenter.x() < 0.0 ||
        settings->captureTarget() == RecordingSettings::CaptureTarget::Window) {
        return fallback;
    }
    QRectF area = screen->geometry();
    if (settings->captureTarget() == RecordingSettings::CaptureTarget::Region) {
        area = QRectF(settings->region()).translated(screen->geometry().topLeft());
    }
    if (!area.contains(m_bubbleDesktopCenter) || area.isEmpty()) {
        return fallback;
    }
    return {(m_bubbleDesktopCenter.x() - area.x()) / area.width(), (m_bubbleDesktopCenter.y() - area.y()) / area.height()};
}

void RecordingController::clearError() {
    setError({});
}

void RecordingController::setState(State state) {
    if (m_state != state) {
        m_state = state;
        // Sample the pointer only while a recording is running (no wake-ups when idle).
        if (m_state == State::Recording || m_state == State::Paused) {
            m_pointerTicker.start();
        } else {
            m_pointerTicker.stop();
        }
        emit stateChanged();
    }
}

void RecordingController::setError(const QString &message) {
    if (m_errorString != message) {
        m_errorString = message;
        emit errorStringChanged();
    }
}

QString RecordingController::nextProjectFolder() const {
    const QDir folder(RecordingSettings::instance()->outputFolder());
    const QString stem = QStringLiteral("ScreenCam %1")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd 'at' HH.mm.ss")));
    QString path = folder.filePath(stem + QStringLiteral(".screencam"));
    for (int i = 2; QFileInfo::exists(path); i++) {
        path = folder.filePath(QStringLiteral("%1 (%2).screencam").arg(stem).arg(i));
    }
    return path;
}

