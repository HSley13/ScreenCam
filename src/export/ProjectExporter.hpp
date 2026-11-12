#pragma once

#include "media/Project.hpp"

#include <QObject>
#include <QString>
#include <QThread>

#include <atomic>
#include <functional>
#include <memory>

/// Renders a project into a finished MP4: screen.mp4 + camera.mp4, with the camera bubble
/// placed per timeline segment, plus the microphone audio from screen.mp4.
///
/// How (one pass, faster than real time on most machines):
///
///   screen.mp4 ──decode──► picture at t ─┐
///                                        ├─► FrameCompositor (the recorder's own painter,
///   camera.mp4 ──decode──► picture at    │    with the segment's bubble at t) ──► NV12
///                t - cameraOffsetMs ─────┘                                      │
///                                                                               ▼
///   screen.mp4 audio packets ──── copied as-is (no re-encode) ──►  MP4 muxer ◄── H.264 encoder
///
/// Using FrameCompositor means the exported bubble is drawn by exactly the same code that
/// draws it while recording, and its geometry is the same C++ the editor preview uses.
class ProjectExporter : public QObject {
    Q_OBJECT

public:
    /// Receives progress 0..1; return false to cancel.
    using ProgressCallback = std::function<bool(double)>;

    explicit ProjectExporter(QObject *parent = nullptr);
    ~ProjectExporter() override;  // cancels and waits for a running export

    /// The whole export, synchronously, on the calling thread (what the worker runs; tests
    /// call it directly). Returns an empty string on success, otherwise what went wrong.
    static QString exportProject(const Project &project, const QString &outputPath,
                                 const ProgressCallback &progress = {});

    /// Starts exporting on a worker thread; progress/finished/failed follow.
    bool start(const Project &project, const QString &outputPath);
    void cancel();
    bool isRunning() const { return m_thread != nullptr; }

    /// The encoder exportProject() would use for this size, e.g. "h264_videotoolbox".
    static QString encoderNameFor(int width, int height);

signals:
    void progress(double ratio);
    void finished(const QString &path);
    void failed(const QString &message);
    void canceled();

private:
    std::unique_ptr<QThread> m_thread;
    std::atomic<bool> m_cancel{false};
};
