#include "export/ProjectExporter.hpp"

#include "core/EncoderPlan.hpp"
#include "core/Nv12.hpp"
#include "export/FFmpegUtil.hpp"
#include "export/VideoFileReader.hpp"
#include "media/FrameCompositor.hpp"

extern "C" {
#include <libavutil/opt.h>
}

#include <QFile>
#include <QFileInfo>

#include <cmath>
#include <vector>

using namespace screencam::ffmpeg;
namespace core = screencam::core;

namespace {

/// Best first: the platform's hardware encoder, libx264 if built in, and mpeg4, which every
/// FFmpeg has, so an export never fails for lack of an H.264 encoder.
std::vector<const char *> encoderCandidates(bool needsHevc) {
    std::vector<const char *> names;
    if (needsHevc) {
        names = {"hevc_videotoolbox", "libx265", "hevc_mf"};
    }
    for (const char *name : {"h264_videotoolbox", "libx264", "h264_mf", "libopenh264", "mpeg4"}) {
        names.push_back(name);
    }
    return names;
}

AVPixelFormat pickPixelFormat(const AVCodec *codec) {
    const AVPixelFormat *formats = nullptr;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
    const void *configs = nullptr;
    int count = 0;
    if (avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs, &count) >= 0) {
        formats = static_cast<const AVPixelFormat *>(configs);
    }
#else
    formats = codec->pix_fmts;
#endif
    if (formats == nullptr) {
        return AV_PIX_FMT_YUV420P;
    }
    bool hasYuv420p = false;
    for (const AVPixelFormat *format = formats; *format != AV_PIX_FMT_NONE; format++) {
        if (*format == AV_PIX_FMT_NV12) {
            return AV_PIX_FMT_NV12;
        }
        if (*format == AV_PIX_FMT_YUV420P) {
            hasYuv420p = true;
        }
    }
    return hasYuv420p ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_NONE;
}

struct OpenedEncoder {
    CodecPtr context;
    QString name;
};

/// `globalHeader`: MP4 wants the codec setup (SPS/PPS) in the file header.
OpenedEncoder openEncoder(const Project &project, bool globalHeader) {
    const core::PixelSize size{project.canvasSize.width(), project.canvasSize.height()};
    const core::EncoderPlan plan = core::planEncoder(size, project.framesPerSecond);
    for (const char *name : encoderCandidates(plan.codec == core::VideoCodec::HEVC)) {
        const AVCodec *codec = avcodec_find_encoder_by_name(name);
        if (codec == nullptr) {
            continue;
        }
        const AVPixelFormat format = pickPixelFormat(codec);
        if (format == AV_PIX_FMT_NONE) {
            continue;
        }
        CodecPtr context(avcodec_alloc_context3(codec));
        context->width = size.width;
        context->height = size.height;
        context->pix_fmt = format;
        context->time_base = AVRational{1, project.framesPerSecond};
        context->framerate = AVRational{project.framesPerSecond, 1};
        context->bit_rate = plan.averageBitRate;
        context->gop_size = plan.maxKeyFrameInterval;
        // Tag the colours as core/Nv12 writes them, or players guess and look washed out.
        context->colorspace = AVCOL_SPC_BT709;
        context->color_primaries = AVCOL_PRI_BT709;
        context->color_trc = AVCOL_TRC_BT709;
        context->color_range = AVCOL_RANGE_MPEG;
        if (globalHeader) {
            context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        if (QByteArray(name) == "libx264") {
            av_opt_set(context->priv_data, "preset", "veryfast", 0);
        }
        if (avcodec_open2(context.get(), codec, nullptr) >= 0) {
            return {std::move(context), QString::fromLatin1(name)};
        }
        // Present but unusable here (no GPU, size unsupported, ...).
    }
    return {};
}

void fillFrame(const QImage &canvas, AVFrame *frame, std::vector<uint8_t> &uvScratch) {
    const int width = frame->width;
    const int height = frame->height;
    if (frame->format == AV_PIX_FMT_NV12) {
        core::convertXrgbToNv12(canvas.constBits(), static_cast<int>(canvas.bytesPerLine()), width, height,
                                frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1]);
        return;
    }
    // YUV420P: NV12's interleaved UV split into two planes.
    const int chromaWidth = (width + 1) / 2;
    const int chromaHeight = (height + 1) / 2;
    uvScratch.resize(static_cast<std::size_t>(chromaWidth) * 2 * static_cast<std::size_t>(chromaHeight));
    core::convertXrgbToNv12(canvas.constBits(), static_cast<int>(canvas.bytesPerLine()), width, height,
                            frame->data[0], frame->linesize[0], uvScratch.data(), chromaWidth * 2);
    for (int y = 0; y < chromaHeight; y++) {
        const uint8_t *row = uvScratch.data() + static_cast<std::size_t>(y) * chromaWidth * 2;
        uint8_t *u = frame->data[1] + static_cast<std::ptrdiff_t>(y) * frame->linesize[1];
        uint8_t *v = frame->data[2] + static_cast<std::ptrdiff_t>(y) * frame->linesize[2];
        for (int x = 0; x < chromaWidth; x++) {
            u[x] = row[x * 2];
            v[x] = row[x * 2 + 1];
        }
    }
}

/// Written under a ".part" name until commit() and deleted on any early return, so a failed
/// export never leaves a broken file or replaces a good one.
class Muxer {
public:
    explicit Muxer(const QString &finalPath)
        : m_finalPath(finalPath)
        , m_partialPath(finalPath + QStringLiteral(".part")) {}

    ~Muxer() {
        close();
        if (!m_committed) {
            QFile::remove(m_partialPath);
        }
    }

    const QString &partialPath() const { return m_partialPath; }

    /// Windows can't delete or rename open files.
    void close() {
        if (m_output == nullptr) {
            return;
        }
        if ((m_output->oformat->flags & AVFMT_NOFILE) == 0) {
            avio_closep(&m_output->pb);
        }
        avformat_free_context(m_output);
        m_output = nullptr;
    }

    bool commit() {
        close();
        QFile::remove(m_finalPath);
        m_committed = QFile::rename(m_partialPath, m_finalPath);
        return m_committed;
    }

    AVFormatContext *m_output = nullptr;

private:
    QString m_finalPath;
    QString m_partialPath;
    bool m_committed = false;
};

/// screen.mp4's audio, copied packet by packet (already AAC: no quality loss).
struct AudioCopy {
    InputPtr input;
    int inputStream = -1;
    AVStream *outputStream = nullptr;
    PacketPtr packet{av_packet_alloc()};
    bool pending = false;  // `packet` holds one not yet written
    bool finished = false;

    bool open(const QString &path, AVFormatContext *output) {
        AVFormatContext *context = nullptr;
        if (avformat_open_input(&context, path.toUtf8().constData(), nullptr, nullptr) < 0) {
            return false;
        }
        input.reset(context);
        if (avformat_find_stream_info(context, nullptr) < 0) {
            return false;
        }
        inputStream = av_find_best_stream(context, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (inputStream < 0) {
            return false;  // recorded without a microphone
        }
        outputStream = avformat_new_stream(output, nullptr);
        avcodec_parameters_copy(outputStream->codecpar, context->streams[inputStream]->codecpar);
        outputStream->codecpar->codec_tag = 0;  // let the MP4 muxer pick its own tag
        outputStream->time_base = context->streams[inputStream]->time_base;
        return true;
    }

    /// Up to `untilMs`, so audio and video stay interleaved in the file.
    int writeUntil(AVFormatContext *output, qint64 untilMs) {
        if (outputStream == nullptr) {
            return 0;
        }
        const AVRational inputBase = input->streams[inputStream]->time_base;
        while (!finished) {
            if (!pending) {
                const int result = av_read_frame(input.get(), packet.get());
                if (result < 0) {
                    finished = true;
                    break;
                }
                if (packet->stream_index != inputStream) {
                    av_packet_unref(packet.get());
                    continue;
                }
                pending = true;
            }
            if (av_compare_ts(packet->dts, inputBase, untilMs, AVRational{1, 1000}) > 0) {
                break;  // belongs after the video written so far
            }
            av_packet_rescale_ts(packet.get(), inputBase, outputStream->time_base);
            packet->stream_index = outputStream->index;
            packet->pos = -1;
            pending = false;
            const int result = av_interleaved_write_frame(output, packet.get());  // takes the packet
            if (result < 0) {
                return result;
            }
        }
        return 0;
    }
};

/// `frame` nullptr flushes.
int encodeAndWrite(AVCodecContext *encoder, const AVFrame *frame, AVFormatContext *output, AVStream *stream,
                   AVPacket *packet) {
    int result = avcodec_send_frame(encoder, frame);
    if (result < 0) {
        return result;
    }
    while (true) {
        result = avcodec_receive_packet(encoder, packet);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
            return 0;
        }
        if (result < 0) {
            return result;
        }
        av_packet_rescale_ts(packet, encoder->time_base, stream->time_base);
        packet->stream_index = stream->index;
        result = av_interleaved_write_frame(output, packet);
        if (result < 0) {
            return result;
        }
    }
}

}  // namespace

ProjectExporter::ProjectExporter(QObject *parent)
    : QObject(parent) {}

ProjectExporter::~ProjectExporter() {
    cancel();
    if (m_thread) {
        m_thread->wait();
    }
}

QString ProjectExporter::encoderNameFor(int width, int height) {
    Project project;
    project.canvasSize = QSize(width, height);
    return openEncoder(project, false).name;
}

QString ProjectExporter::exportProject(const Project &project, const QString &outputPath,
                                       const ProgressCallback &progress) {
    if (project.segments.empty() || project.durationMs <= 0) {
        return QStringLiteral("The project is empty.");
    }
    if (project.canvasSize.width() < 2 || project.canvasSize.height() < 2) {
        return QStringLiteral("The project has no video size.");
    }
    const bool withCamera = !project.cameraFile.isEmpty();

    QString error;
    VideoFileReader screen;
    if (!screen.open(project.screenPath(), &error)) {
        return QStringLiteral("Couldn't read the screen recording: %1").arg(error);
    }
    VideoFileReader camera;
    if (withCamera && !camera.open(project.cameraPath(), &error)) {
        return QStringLiteral("Couldn't read the camera recording: %1").arg(error);
    }

    Muxer muxer(outputPath);
    const QString &partialPath = muxer.partialPath();
    int result = avformat_alloc_output_context2(&muxer.m_output, nullptr, "mp4", partialPath.toUtf8().constData());
    if (result < 0) {
        return errorText(result);
    }
    AVFormatContext *output = muxer.m_output;

    OpenedEncoder encoder = openEncoder(project, (output->oformat->flags & AVFMT_GLOBALHEADER) != 0);
    if (!encoder.context) {
        return QStringLiteral("No usable video encoder in this FFmpeg build.");
    }
    AVStream *videoStream = avformat_new_stream(output, nullptr);
    avcodec_parameters_from_context(videoStream->codecpar, encoder.context.get());
    videoStream->time_base = encoder.context->time_base;
    videoStream->avg_frame_rate = encoder.context->framerate;

    AudioCopy audio;
    audio.open(project.screenPath(), output);  // none is fine: silent video

    result = avio_open(&output->pb, partialPath.toUtf8().constData(), AVIO_FLAG_WRITE);
    if (result < 0) {
        return QStringLiteral("Couldn't create %1: %2").arg(partialPath, errorText(result));
    }
    // "faststart": index at the front, so the video plays before it has fully downloaded.
    AVDictionary *options = nullptr;
    av_dict_set(&options, "movflags", "+faststart", 0);
    result = avformat_write_header(output, &options);
    av_dict_free(&options);
    if (result < 0) {
        return errorText(result);
    }

    FrameCompositor compositor;
    CompositorConfig config;
    config.canvasSize = project.canvasSize;
    config.layout = CompositeLayout::ScreenWithBubble;
    config.framesPerSecond = project.framesPerSecond;
    compositor.configure(config);

    FramePtr frame(av_frame_alloc());
    frame->format = encoder.context->pix_fmt;
    frame->width = encoder.context->width;
    frame->height = encoder.context->height;
    frame->colorspace = AVCOL_SPC_BT709;
    frame->color_range = AVCOL_RANGE_MPEG;
    result = av_frame_get_buffer(frame.get(), 0);
    if (result < 0) {
        return errorText(result);
    }
    PacketPtr packet(av_packet_alloc());
    std::vector<uint8_t> uvScratch;

    const qint64 frameCount =
        std::max<qint64>(1, static_cast<qint64>(std::ceil(project.durationMs * project.framesPerSecond / 1000.0)));
    for (qint64 index = 0; index < frameCount; index++) {
        const qint64 timeMs = index * 1000 / project.framesPerSecond;
        const ProjectSegment &segment = project.segments.at(static_cast<std::size_t>(project.segmentAt(timeMs)));

        compositor.setScreenImage(screen.imageAt(timeMs));
        if (withCamera && segment.cameraVisible) {
            compositor.setCameraImage(camera.imageAt(std::max<qint64>(0, timeMs - project.cameraOffsetMs)));
            compositor.setBubble(segment.style);
        } else {
            compositor.setCameraImage(QImage());  // no bubble
        }
        const QImage canvas = compositor.renderCanvas();

        // The encoder may still hold on to the previous frame's buffer: get our own copy.
        result = av_frame_make_writable(frame.get());
        if (result < 0) {
            return errorText(result);
        }
        fillFrame(canvas, frame.get(), uvScratch);
        frame->pts = index;
        result = encodeAndWrite(encoder.context.get(), frame.get(), output, videoStream, packet.get());
        if (result >= 0) {
            result = audio.writeUntil(output, timeMs);
        }
        if (result < 0) {
            return QStringLiteral("Encoding failed: %1").arg(errorText(result));
        }
        if (progress && !progress(static_cast<double>(index + 1) / static_cast<double>(frameCount))) {
            return QStringLiteral("canceled");  // ~Muxer deletes the partial file
        }
    }

    result = encodeAndWrite(encoder.context.get(), nullptr, output, videoStream, packet.get());  // flush
    if (result >= 0) {
        result = audio.writeUntil(output, project.durationMs + 1000);  // audio tail
    }
    if (result >= 0) {
        result = av_write_trailer(output);
    }
    if (result < 0) {
        return QStringLiteral("Finishing the file failed: %1").arg(errorText(result));
    }
    if (!muxer.commit()) {
        return QStringLiteral("Couldn't rename %1 to %2").arg(partialPath, QFileInfo(outputPath).fileName());
    }
    return QString();
}

bool ProjectExporter::start(const Project &project, const QString &outputPath) {
    if (m_thread) {
        return false;
    }
    m_cancel = false;
    // The worker uses its own copy of the project and reports through queued signals.
    m_thread.reset(QThread::create([this, project, outputPath] {
        int lastPercent = -1;
        const QString error = exportProject(project, outputPath, [this, &lastPercent](double ratio) {
            const int percent = static_cast<int>(ratio * 100.0);
            if (percent != lastPercent) {  // ~100 updates, not one per frame
                lastPercent = percent;
                QMetaObject::invokeMethod(this, [this, ratio] { emit progress(ratio); }, Qt::QueuedConnection);
            }
            return !m_cancel.load();
        });
        QMetaObject::invokeMethod(
            this,
            [this, error, outputPath] {
                m_thread->wait();
                m_thread.reset();
                if (error.isEmpty()) {
                    emit finished(outputPath);
                } else if (m_cancel) {
                    emit canceled();
                } else {
                    emit failed(error);
                }
            },
            Qt::QueuedConnection);
    }));
    m_thread->start(QThread::LowPriority);
    return true;
}

void ProjectExporter::cancel() {
    m_cancel = true;
}
