#pragma once

#include "media/Project.hpp"

#include <QObject>
#include <QString>
#include <QThread>

#include <atomic>
#include <functional>
#include <memory>

/// Renders a project into one MP4 in a single pass: both videos are decoded, painted by the
/// recorder's own FrameCompositor (so the bubble looks exactly as previewed) and re-encoded;
/// the audio is copied as-is.
class ProjectExporter : public QObject {
    Q_OBJECT

public:
    /// Receives progress 0..1; return false to cancel.
    using ProgressCallback = std::function<bool(double)>;

    explicit ProjectExporter(QObject *parent = nullptr);
    ~ProjectExporter() override;  // cancels and waits for a running export

    /// Synchronous (what the worker runs; tests call it). Empty on success, else the error.
    static QString exportProject(const Project &project, const QString &outputPath,
                                 const ProgressCallback &progress = {});

    bool start(const Project &project, const QString &outputPath);
    void cancel();
    bool isRunning() const { return m_thread != nullptr; }

    /// e.g. "h264_videotoolbox"
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
