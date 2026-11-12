#pragma once

#include "export/FFmpegUtil.hpp"

#include <QImage>
#include <QString>

/// Decodes one video file front to back and answers "what picture is showing at time t?".
///
/// The exporter walks the timeline in steady steps (1/30 s ...) while the file's own frames
/// may come at a slightly different, uneven rate. So instead of "give me frame n", it asks
/// imageAt(t): the latest frame whose timestamp is <= t — exactly what a player would show.
/// Times only go forward, so the file is decoded once, sequentially (no seeking).
class VideoFileReader {
public:
    bool open(const QString &path, QString *error);

    /// The picture showing at `timeMs` (RGB32). Before the first frame: the first frame;
    /// after the last: the last one. Calls must use non-decreasing times.
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

    bool m_hasPending = false;  // m_frame holds a decoded frame not yet turned into m_current
    qint64 m_pendingMs = 0;
    QImage m_current;
};
