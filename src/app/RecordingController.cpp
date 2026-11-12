#include "app/RecordingController.hpp"

#include "app/BubbleStyle.hpp"
#include "app/CaptureSources.hpp"
#include "app/DeviceCatalog.hpp"
#include "app/RecordingSettings.hpp"
#include "core/EncoderPlan.hpp"
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

    m_elapsedTicker.setInterval(250);
    connect(&m_elapsedTicker, &QTimer::timeout, this, &RecordingController::elapsedChanged);

    // Live inputs -> pipeline (only while it records).
    CaptureSources *capture = CaptureSources::instance();
    connect(capture, &CaptureSources::screenFrameArrived, this, [this](const QVideoFrame &frame) {
        if (m_pipeline.isActive()) {
            m_pipeline.pushScreenFrame(frame);
        }
    });
    connect(capture, &CaptureSources::cameraFrameArrived, this, [this](const QVideoFrame &frame) {
        if (m_pipeline.isActive()) {
            m_pipeline.pushCameraFrame(frame);
        }
    });
    // Dragging/restyling the bubble during a recording shows up in the video.
    connect(BubbleStyle::instance(), &BubbleStyle::styleChanged, this, [this] {
        if (m_pipeline.isActive()) {
            m_pipeline.updateBubble(BubbleStyle::instance()->snapshot());
        }
    });

    connect(&m_pipeline, &RecordingPipeline::finished, this, [this](const QString &path) {
        m_lastRecording = path;
        emit lastRecordingChanged();
        finish();
    });
    connect(&m_pipeline, &RecordingPipeline::failed, this, [this](const QString &message) {
        setError(message);
        finish();
    });
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

    // Recording feeds frames through QVideoFrameInput, which needs the FFmpeg backend in every mode.
    if (!capture->backendProblem().isEmpty()) {
        return capture->backendProblem();
    }
    if (settings->mode() == RecordingSettings::CaptureMode::CameraOnly) {
        if (settings->cameraId().isEmpty()) {
            return tr("Choose a camera to record in Camera mode.");
        }
        if (!capture->cameraActive()) {
            return capture->cameraError().isEmpty() ? tr("The camera isn't ready yet.") : capture->cameraError();
        }
    } else if (!capture->screenError().isEmpty()) {
        return capture->screenError();
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
    const bool hide = settings->hideWindowWhileRecording();
    if (hide) {
        emit windowHideRequested();
    }
    setState(State::Recording);

    QTimer::singleShot(hide ? kHideDelayMs : 0, this, [this] {
        if (m_state != State::Recording || m_pipeline.isActive()) {
            return;  // stopped during the delay
        }
        const RecordingSettings *settings = RecordingSettings::instance();
        const CaptureSources *capture = CaptureSources::instance();

        RecordingPipeline::Request request;
        request.compositor.canvasSize = capture->outputSize();
        request.compositor.sourceCrop = capture->sourceCrop();
        request.compositor.framesPerSecond = settings->frameRate();
        request.compositor.bubble = BubbleStyle::instance()->snapshot();
        switch (settings->mode()) {
        case RecordingSettings::CaptureMode::CameraOnly:
            request.compositor.layout = CompositeLayout::CameraOnly;
            break;
        case RecordingSettings::CaptureMode::ScreenOnly:
            request.compositor.layout = CompositeLayout::ScreenOnly;
            break;
        case RecordingSettings::CaptureMode::ScreenAndCamera:
            request.compositor.layout =
                settings->usesCamera() ? CompositeLayout::ScreenWithBubble : CompositeLayout::ScreenOnly;
            break;
        }
        if (DeviceCatalog::instance()->microphoneAllowed()) {
            request.microphone = DeviceCatalog::findMicrophone(settings->microphoneId());
        }
        const QSize size = request.compositor.canvasSize;
        request.videoBitRate =
            core::planEncoder(core::PixelSize{size.width(), size.height()}, request.compositor.framesPerSecond)
                .averageBitRate;
        request.outputPath = nextOutputPath();

        m_clock.start();
        m_pausedMs = 0;
        m_pauseStartedMs = -1;
        m_elapsedTicker.start();
        emit elapsedChanged();
        if (!m_pipeline.start(request)) {
            finish();  // no-op if failed() already finished us
        }
    });
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
        m_elapsedTicker.stop();
        setState(State::Finalizing);
        m_pipeline.stop();
        break;
    case State::Idle:
    case State::Finalizing:
        break;
    }
}

void RecordingController::togglePause() {
    if (m_state == State::Recording && m_pipeline.isActive()) {
        m_pipeline.pause();
        m_pauseStartedMs = m_clock.elapsed();
        setState(State::Paused);
    } else if (m_state == State::Paused) {
        m_pipeline.resume();
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

void RecordingController::openLastRecording() const {
    if (!m_lastRecording.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastRecording));
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
        emit stateChanged();
    }
}

void RecordingController::setError(const QString &message) {
    if (m_errorString != message) {
        m_errorString = message;
        emit errorStringChanged();
    }
}

QString RecordingController::nextOutputPath() const {
    // "ScreenCam 2026-11-12 at 18.42.05.mp4" — sorts by date, no characters Windows rejects.
    const QDir folder(RecordingSettings::instance()->outputFolder());
    const QString stem = QStringLiteral("ScreenCam %1")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd 'at' HH.mm.ss")));
    QString path = folder.filePath(stem + QStringLiteral(".mp4"));
    for (int i = 2; QFileInfo::exists(path); i++) {
        path = folder.filePath(QStringLiteral("%1 (%2).mp4").arg(stem).arg(i));
    }
    return path;
}
