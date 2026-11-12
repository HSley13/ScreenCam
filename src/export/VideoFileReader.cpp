#include "export/VideoFileReader.hpp"

extern "C" {
#include <libavutil/pixdesc.h>
}

using namespace screencam::ffmpeg;

bool VideoFileReader::open(const QString &path, QString *error) {
    AVFormatContext *input = nullptr;
    int result = avformat_open_input(&input, path.toUtf8().constData(), nullptr, nullptr);
    if (result < 0) {
        *error = QStringLiteral("%1: %2").arg(path, errorText(result));
        return false;
    }
    m_input.reset(input);
    result = avformat_find_stream_info(input, nullptr);
    if (result < 0) {
        *error = errorText(result);
        return false;
    }

    const AVCodec *codec = nullptr;
    m_stream = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (m_stream < 0 || codec == nullptr) {
        *error = QStringLiteral("%1 has no video").arg(path);
        return false;
    }
    m_decoder.reset(avcodec_alloc_context3(codec));
    avcodec_parameters_to_context(m_decoder.get(), input->streams[m_stream]->codecpar);
    m_decoder->thread_count = 0;  // 0 = as many threads as makes sense
    result = avcodec_open2(m_decoder.get(), codec, nullptr);
    if (result < 0) {
        *error = errorText(result);
        return false;
    }
    m_frame.reset(av_frame_alloc());
    m_packet.reset(av_packet_alloc());
    return true;
}

const QImage &VideoFileReader::imageAt(qint64 timeMs) {
    // Advance while the next frame is already due. The first frame is always taken, so a
    // time before the video starts still gets a picture.
    while (true) {
        if (!m_hasPending && !decodeNext()) {
            break;  // end of file: keep showing the last frame
        }
        if (m_pendingMs <= timeMs || m_current.isNull()) {
            convertPending();
            continue;
        }
        break;  // the next frame belongs to a later time; keep it for then
    }
    return m_current;
}

bool VideoFileReader::decodeNext() {
    if (m_finished) {
        return false;
    }
    // FFmpeg's decode loop: ask for a frame; if the decoder needs more input, feed it the
    // next packet of our stream; at the end of the file send "nullptr" to flush it.
    while (true) {
        int result = avcodec_receive_frame(m_decoder.get(), m_frame.get());
        if (result == 0) {
            const AVStream *stream = m_input->streams[m_stream];
            const int64_t pts = m_frame->best_effort_timestamp != AV_NOPTS_VALUE ? m_frame->best_effort_timestamp
                                                                                 : m_frame->pts;
            m_pendingMs = pts == AV_NOPTS_VALUE ? 0 : av_rescale_q(pts, stream->time_base, AVRational{1, 1000});
            m_hasPending = true;
            return true;
        }
        if (result != AVERROR(EAGAIN) || m_draining) {
            m_finished = true;  // AVERROR_EOF, or a decoding error: stop here
            return false;
        }
        result = av_read_frame(m_input.get(), m_packet.get());
        if (result < 0) {
            m_draining = true;
            avcodec_send_packet(m_decoder.get(), nullptr);
            continue;
        }
        if (m_packet->stream_index == m_stream) {
            avcodec_send_packet(m_decoder.get(), m_packet.get());
        }
        av_packet_unref(m_packet.get());
    }
}

void VideoFileReader::convertPending() {
    const AVFrame *frame = m_frame.get();
    const auto format = static_cast<AVPixelFormat>(frame->format);
    m_scale.reset(sws_getCachedContext(m_scale.release(), frame->width, frame->height, format, frame->width,
                                       frame->height, AV_PIX_FMT_RGB32, SWS_BILINEAR, nullptr, nullptr, nullptr));
    if (m_scale) {
        // Read the colours the way they were written: BT.601 for SD-style tags, BT.709
        // otherwise (what ScreenCam records), limited ("video") range unless tagged full.
        const bool bt601 = frame->colorspace == AVCOL_SPC_BT470BG || frame->colorspace == AVCOL_SPC_SMPTE170M;
        const bool fullRange = frame->color_range == AVCOL_RANGE_JPEG;
        const int *source = sws_getCoefficients(bt601 ? SWS_CS_ITU601 : SWS_CS_ITU709);
        sws_setColorspaceDetails(m_scale.get(), source, fullRange ? 1 : 0, sws_getCoefficients(SWS_CS_DEFAULT), 1, 0,
                                 1 << 16, 1 << 16);

        // AV_PIX_FMT_RGB32 is native-endian 0xAARRGGBB words: the same layout as QImage's.
        QImage image(frame->width, frame->height, QImage::Format_RGB32);
        uint8_t *destination[4] = {image.bits(), nullptr, nullptr, nullptr};
        const int destinationStride[4] = {static_cast<int>(image.bytesPerLine()), 0, 0, 0};
        sws_scale(m_scale.get(), frame->data, frame->linesize, 0, frame->height, destination, destinationStride);
        m_current = image;
    }
    av_frame_unref(m_frame.get());
    m_hasPending = false;
}
