#include "media/RecordingPipeline.hpp"

#include <QFileInfo>
#include <QMediaFormat>
#include <QUrl>

RecordingPipeline::RecordingPipeline(QObject *parent)
    : QObject(parent) {
    m_session.setRecorder(&m_recorder);
    m_session.setVideoFrameInput(&m_videoInput);

    // The compositor paints on its own thread; it is deleted when that thread ends.
    m_compositor = new FrameCompositor;
    m_compositor->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_compositor, &QObject::deleteLater);
    m_thread.setObjectName(QStringLiteral("ScreenCam compositor"));
    m_thread.start();

    // Different threads -> queued: frames arrive here on the main thread.
    connect(m_compositor, &FrameCompositor::frameReady, this, &RecordingPipeline::sendFrame);
    connect(m_compositor, &FrameCompositor::clockStarted, this, &RecordingPipeline::clockStarted);
    connect(&m_recorder, &QMediaRecorder::recorderStateChanged, this, &RecordingPipeline::onRecorderStateChanged);

    // Long recordings can take a few seconds to close (the MP4 index is written at the end),
    // so be generous before declaring the encoder stuck.
    m_stopTimeout.setSingleShot(true);
    m_stopTimeout.setInterval(20000);
    connect(&m_stopTimeout, &QTimer::timeout, this, [this] {
        if (m_active && m_error.isEmpty()) {
            m_error = tr("Saving the recording took too long; the file may be incomplete.");
        }
        finish();
    });
    connect(&m_recorder, &QMediaRecorder::errorOccurred, this, [this](QMediaRecorder::Error, const QString &message) {
        m_error = message.isEmpty() ? tr("The recording failed.") : message;
        if (m_recorder.recorderState() == QMediaRecorder::StoppedState) {
            finish();  // never got going: no state change will follow
        } else {
            stop();
        }
    });
}

RecordingPipeline::~RecordingPipeline() {
    if (m_active) {
        onCompositor([](FrameCompositor *c) { c->stop(); }, Qt::BlockingQueuedConnection);
        m_recorder.stop();
    }
    m_thread.quit();
    m_thread.wait();
}

template <typename Fn>
void RecordingPipeline::onCompositor(Fn &&fn, Qt::ConnectionType type) {
    FrameCompositor *compositor = m_compositor;
    QMetaObject::invokeMethod(
        compositor, [compositor, fn = std::forward<Fn>(fn)] { fn(compositor); }, type);
}

bool RecordingPipeline::start(const Request &request) {
    if (m_active) {
        return false;
    }
    if (request.compositor.canvasSize.isEmpty() || request.outputPath.isEmpty()) {
        emit failed(tr("Nothing to record: no screen or camera picture is available."));
        return false;
    }

    m_error.clear();
    m_droppedFrames = 0;
    m_paused = false;
    m_stopping = false;
    m_outputPath = request.outputPath;

    // MP4 + H.264 + AAC plays everywhere (QuickTime, Windows, browsers, phones).
    QMediaFormat format(QMediaFormat::MPEG4);
    format.setVideoCodec(QMediaFormat::VideoCodec::H264);
    format.setAudioCodec(QMediaFormat::AudioCodec::AAC);
    m_recorder.setMediaFormat(format);
    m_recorder.setEncodingMode(QMediaRecorder::AverageBitRateEncoding);
    m_recorder.setVideoBitRate(request.videoBitRate);
    m_recorder.setVideoFrameRate(request.compositor.framesPerSecond);
    m_recorder.setVideoResolution(request.compositor.canvasSize);
    m_recorder.setAudioSampleRate(48000);
    m_recorder.setAudioBitRate(128000);
    m_recorder.setOutputLocation(QUrl::fromLocalFile(request.outputPath));

    m_session.setAudioInput(nullptr);
    m_audioInput.reset();
    if (request.microphone) {
        m_audioInput = std::make_unique<QAudioInput>(*request.microphone);
        m_session.setAudioInput(m_audioInput.get());
    }

    // Configure synchronously so the very first tick already uses the right canvas.
    const CompositorConfig config = request.compositor;
    onCompositor([config](FrameCompositor *c) { c->configure(config); }, Qt::BlockingQueuedConnection);

    m_active = true;
    m_recorder.record();
    // The clock starts with the recorder, so video time 0 matches the start of the audio.
    onCompositor([](FrameCompositor *c) { c->start(); });
    return true;
}

void RecordingPipeline::pause() {
    if (!m_active || m_paused || m_stopping) {
        return;
    }
    m_paused = true;
    onCompositor([](FrameCompositor *c) { c->pause(); }, Qt::BlockingQueuedConnection);
    m_recorder.pause();
}

void RecordingPipeline::resume() {
    if (!m_active || !m_paused || m_stopping) {
        return;
    }
    m_paused = false;
    m_recorder.record();  // record() on a paused recorder resumes it
    onCompositor([](FrameCompositor *c) { c->resume(); });
}

void RecordingPipeline::stop() {
    if (!m_active || m_stopping) {
        return;
    }
    m_stopping = true;
    // Blocking: once this returns, the compositor emits no more frames.
    onCompositor([](FrameCompositor *c) { c->stop(); }, Qt::BlockingQueuedConnection);
    if (m_recorder.recorderState() == QMediaRecorder::StoppedState) {
        // The recorder never got going (or already ended): it won't report "stopped" again,
        // so finish now instead of waiting for a signal that never comes.
        finish();
        return;
    }
    // Normally the recorder reports StoppedState within a second. If the encoder hangs, don't
    // leave the app stuck in "Saving…": give up after a while and report it.
    m_stopTimeout.start();
    m_recorder.stop();
}

void RecordingPipeline::sendFrame(const QVideoFrame &frame) {
    if (!m_active || m_stopping || m_paused) {
        return;
    }
    // false = the encoder's queue is full (or not ready yet): drop rather than block.
    if (!m_videoInput.sendVideoFrame(frame)) {
        m_droppedFrames++;
    }
}

void RecordingPipeline::onRecorderStateChanged(QMediaRecorder::RecorderState state) {
    if (state == QMediaRecorder::StoppedState && m_active) {
        finish();
    }
}

void RecordingPipeline::finish() {
    if (!m_active) {
        return;
    }
    m_active = false;
    m_paused = false;
    m_stopping = false;
    m_stopTimeout.stop();
    onCompositor([](FrameCompositor *c) { c->stop(); });
    m_session.setAudioInput(nullptr);
    m_audioInput.reset();

    const QUrl actual = m_recorder.actualLocation();
    const QString path = actual.isLocalFile() ? actual.toLocalFile() : m_outputPath;
    if (!m_error.isEmpty()) {
        emit failed(m_error);
    } else if (!QFileInfo::exists(path)) {
        emit failed(tr("The recording could not be saved to %1.").arg(path));
    } else {
        emit finished(path);
    }
}

void RecordingPipeline::pushScreenFrame(const QVideoFrame &frame) {
    onCompositor([frame](FrameCompositor *c) { c->setScreenFrame(frame); });
}

void RecordingPipeline::pushCameraFrame(const QVideoFrame &frame) {
    onCompositor([frame](FrameCompositor *c) { c->setCameraFrame(frame); });
}

void RecordingPipeline::pushScreenImage(const QImage &image) {
    onCompositor([image](FrameCompositor *c) { c->setScreenImage(image); });
}

void RecordingPipeline::pushCameraImage(const QImage &image) {
    onCompositor([image](FrameCompositor *c) { c->setCameraImage(image); });
}

void RecordingPipeline::pushPointer(const PointerOverlay &pointer) {
    if (!m_active) {
        return;
    }
    onCompositor([pointer](FrameCompositor *c) { c->setPointer(pointer); });
}

void RecordingPipeline::updateBubble(const BubbleStyleSnapshot &bubble) {
    onCompositor([bubble](FrameCompositor *c) { c->setBubble(bubble); });
}
