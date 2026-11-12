#pragma once

// Small helpers shared by the exporter's reader and writer: RAII owners for FFmpeg's C
// structs (so every early `return` frees them) and readable error strings.

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

#include <QString>

#include <memory>

namespace screencam::ffmpeg {

struct InputDeleter {
    void operator()(AVFormatContext *context) const { avformat_close_input(&context); }
};
struct CodecDeleter {
    void operator()(AVCodecContext *context) const { avcodec_free_context(&context); }
};
struct FrameDeleter {
    void operator()(AVFrame *frame) const { av_frame_free(&frame); }
};
struct PacketDeleter {
    void operator()(AVPacket *packet) const { av_packet_free(&packet); }
};
struct ScaleDeleter {
    void operator()(SwsContext *context) const { sws_freeContext(context); }
};

using InputPtr = std::unique_ptr<AVFormatContext, InputDeleter>;
using CodecPtr = std::unique_ptr<AVCodecContext, CodecDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using ScalePtr = std::unique_ptr<SwsContext, ScaleDeleter>;

/// "Invalid data found when processing input" instead of -1094995529.
inline QString errorText(int code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

}  // namespace screencam::ffmpeg
