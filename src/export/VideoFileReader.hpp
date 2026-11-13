#pragma once

#include "export/FFmpegUtil.hpp"

#include <QImage>
#include <QString>

/// "Which picture shows at time t?" — the latest frame at or before t, as a player would show
/// it, since the file's frame rate may be uneven. Times only go forward, so the file is
/// decoded once, front to back.
class VideoFileReader {
public:
    bool open(const QString &path, QString *error);

    /// RGB32; clamps to the first and last frame. Times must not decrease.
    const QImage &imageAt(qint64 timeMs);

private:
    bool decodeNext();  // next frame into m_frame; false at the end of the file
    void convertPending();

    screencam::ffmpeg::InputPtr m_input;
    screencam::ffmpeg::CodecPtr m_decoder;
    screencam::ffmpeg::FramePtr m_frame;
    screencam::ffmpeg::PacketPtr m_packet;
    screencam::ffmpeg::ScalePtr m_scale;
    int m_stream = -1;
    bool m_draining = false;  // the demuxer is done; the decoder hands out what it buffered
    bool m_finished = false;

    bool m_hasPending = false;  // m_frame not yet turned into m_current
    qint64 m_pendingMs = 0;
    QImage m_current;
};
