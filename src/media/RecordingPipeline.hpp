#pragma once

#include "media/FrameCompositor.hpp"

#include <QAudioDevice>
#include <QAudioInput>
#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVideoFrameInput>

#include <memory>
#include <optional>

/// Frames -> FrameCompositor (worker thread) -> QVideoFrameInput, plus the microphone,
/// -> QMediaRecorder (H.264 + AAC in MP4). Independent of QML, so tests can drive it.
class RecordingPipeline : public QObject {
    Q_OBJECT

public:
    struct Request {
        CompositorConfig compositor;
        QString outputPath;
        std::optional<QAudioDevice> microphone;  // nullopt = silent video
        int videoBitRate = 8'000'000;
    };

    explicit RecordingPipeline(QObject *parent = nullptr);
    ~RecordingPipeline() override;

    /// False (and failed()) if the request is unusable.
    bool start(const Request &request);
    void pause();
    void resume();
    /// finished() or failed() follows once the file is closed.
    void stop();

    bool isActive() const { return m_active; }
    bool isPaused() const { return m_paused; }
    int droppedFrames() const { return m_droppedFrames; }

    // Call from the main thread.
    void pushScreenFrame(const QVideoFrame &frame);
    void pushCameraFrame(const QVideoFrame &frame);
    void pushScreenImage(const QImage &image);
    void pushCameraImage(const QImage &image);
    void updateBubble(const BubbleStyleSnapshot &bubble);
    void pushPointer(const PointerOverlay &pointer);

signals:
    void clockStarted(qint64 referenceMs);
    void finished(const QString &path);
    void failed(const QString &message);

private:
    void sendFrame(const QVideoFrame &frame);
    void onRecorderStateChanged(QMediaRecorder::RecorderState state);
    void finish();
    template <typename Fn>
    void onCompositor(Fn &&fn, Qt::ConnectionType type = Qt::QueuedConnection);

    QMediaCaptureSession m_session;
    QMediaRecorder m_recorder;
    QVideoFrameInput m_videoInput;
    std::unique_ptr<QAudioInput> m_audioInput;

    QThread m_thread;
    FrameCompositor *m_compositor = nullptr;  // lives on m_thread

    QString m_outputPath;
    QString m_error;
    bool m_active = false;
    bool m_paused = false;
    bool m_stopping = false;
    QTimer m_stopTimeout;  // stop() never waits forever for the encoder
    int m_droppedFrames = 0;
};
