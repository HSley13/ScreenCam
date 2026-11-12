#include "media/FrameCompositor.hpp"

#include "core/BubbleGeometry.hpp"
#include "core/Nv12.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QVideoFrameFormat>

#include <algorithm>
#include <cmath>
#include <vector>

namespace core = screencam::core;

namespace {

const QColor kCanvasBackground(7, 8, 11);
const QColor kCameraPlaceholder(29, 33, 48);
constexpr int kShadowAlpha = 140;

/// One horizontal + one vertical box-blur pass over the alpha channel of a black,
/// premultiplied image. Repeating it twice approximates a Gaussian blur closely.
void boxBlurAlpha(QImage &image, int radius) {
    const int width = image.width();
    const int height = image.height();
    const int window = radius * 2 + 1;
    std::vector<int> line(static_cast<size_t>(std::max(width, height)));

    for (int y = 0; y < height; y++) {
        auto *row = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < width; x++) {
            line[static_cast<size_t>(x)] = qAlpha(row[x]);
        }
        int sum = 0;
        for (int i = -radius; i <= radius; i++) {
            sum += line[static_cast<size_t>(std::clamp(i, 0, width - 1))];
        }
        for (int x = 0; x < width; x++) {
            row[x] = qRgba(0, 0, 0, sum / window);
            sum += line[static_cast<size_t>(std::min(x + radius + 1, width - 1))];
            sum -= line[static_cast<size_t>(std::max(x - radius, 0))];
        }
    }

    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            line[static_cast<size_t>(y)] = qAlpha(reinterpret_cast<const QRgb *>(image.constScanLine(y))[x]);
        }
        int sum = 0;
        for (int i = -radius; i <= radius; i++) {
            sum += line[static_cast<size_t>(std::clamp(i, 0, height - 1))];
        }
        for (int y = 0; y < height; y++) {
            reinterpret_cast<QRgb *>(image.scanLine(y))[x] = qRgba(0, 0, 0, sum / window);
            sum += line[static_cast<size_t>(std::min(y + radius + 1, height - 1))];
            sum -= line[static_cast<size_t>(std::max(y - radius, 0))];
        }
    }
}

}  // namespace

FrameCompositor::FrameCompositor(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this)) {  // child object: moves to the worker thread with us
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &FrameCompositor::tick);
}

void FrameCompositor::configure(const CompositorConfig &config) {
    m_config = config;
    m_config.framesPerSecond = std::clamp(config.framesPerSecond, 1, 120);
    m_shadowKeyRadius = -1.0;  // force a new shadow and mask
    m_maskKeyRadius = -1.0;
}

void FrameCompositor::setBubble(const BubbleStyleSnapshot &bubble) {
    m_config.bubble = bubble;
}

void FrameCompositor::setScreenFrame(const QVideoFrame &frame) {
    m_screenFrame = frame;
    m_screenDirty = true;
}

void FrameCompositor::setCameraFrame(const QVideoFrame &frame) {
    m_cameraFrame = frame;
    m_cameraDirty = true;
}

void FrameCompositor::setScreenImage(const QImage &image) {
    m_screenImage = image;
    m_screenDirty = false;
}

void FrameCompositor::setCameraImage(const QImage &image) {
    m_cameraImage = image;
    m_cameraDirty = false;
}

void FrameCompositor::refreshImages() {
    // A camera at 30 fps and a 60 Hz screen would otherwise cost a conversion per frame
    // even when we only record 24 fps: convert lazily, only what a tick actually uses.
    if (m_screenDirty) {
        m_screenImage = m_screenFrame.toImage();
        m_screenDirty = false;
    }
    if (m_cameraDirty) {
        m_cameraImage = m_cameraFrame.toImage();
        m_cameraDirty = false;
    }
}

QImage FrameCompositor::renderCanvas() {
    refreshImages();

    QImage canvas(m_config.canvasSize, QImage::Format_RGB32);
    if (canvas.isNull()) {
        return canvas;
    }
    canvas.fill(kCanvasBackground);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (m_config.layout == CompositeLayout::CameraOnly) {
        drawCamera(painter, QRectF(canvas.rect()), m_config.bubble.mirrored);
        return canvas;
    }

    drawScreen(painter, QRectF(canvas.rect()));
    if (m_config.layout == CompositeLayout::ScreenWithBubble && !m_cameraImage.isNull()) {
        drawBubble(painter);
    }
    return canvas;
}

void FrameCompositor::drawScreen(QPainter &painter, const QRectF &canvas) const {
    if (m_screenImage.isNull()) {
        return;
    }
    // 1) The part of the frame to record (all of it, or the selected region).
    const QRectF crop = m_config.sourceCrop.isValid() ? m_config.sourceCrop : QRectF(0.0, 0.0, 1.0, 1.0);
    const QRectF source(crop.x() * m_screenImage.width(), crop.y() * m_screenImage.height(),
                        crop.width() * m_screenImage.width(), crop.height() * m_screenImage.height());
    // 2) Fit it into the canvas. Screens and regions already have the canvas's aspect ratio, so
    //    this fills it; a window resized mid-recording gets black bars instead of being stretched.
    const core::RectD fit = core::aspectFitRect(core::SizeD{source.width(), source.height()},
                                                core::SizeD{canvas.width(), canvas.height()});
    painter.drawImage(QRectF(canvas.x() + fit.x, canvas.y() + fit.y, fit.width, fit.height), m_screenImage, source);
}

void FrameCompositor::drawCamera(QPainter &painter, const QRectF &target, bool mirrored) const {
    if (m_cameraImage.isNull() || target.isEmpty()) {
        return;
    }
    // Crop the middle of the camera image to the target's aspect ratio (no stretching).
    const core::RectD crop =
        core::aspectFillCrop(core::SizeD{static_cast<double>(m_cameraImage.width()),
                                         static_cast<double>(m_cameraImage.height())},
                             target.width() / target.height());
    const QRectF source(crop.x, crop.y, crop.width, crop.height);

    painter.save();
    if (mirrored) {
        // Flip horizontally around the target's vertical centre line.
        painter.translate(target.center().x(), 0.0);
        painter.scale(-1.0, 1.0);
        painter.translate(-target.center().x(), 0.0);
    }
    painter.drawImage(target, m_cameraImage, source);
    painter.restore();
}

void FrameCompositor::drawBubble(QPainter &painter) {
    const BubbleStyleSnapshot &style = m_config.bubble;
    const double canvasWidth = m_config.canvasSize.width();
    const double canvasHeight = m_config.canvasSize.height();

    // Same geometry the QML preview uses.
    const core::RectD r =
        core::bubbleRect(core::SizeD{canvasWidth, canvasHeight}, style.shape, style.size, style.center);
    const QRect rect(qRound(r.x), qRound(r.y), qRound(r.width), qRound(r.height));
    if (rect.isEmpty()) {
        return;
    }
    const double radius = core::bubbleCornerRadius(
        style.shape, core::SizeD{static_cast<double>(rect.width()), static_cast<double>(rect.height())},
        style.roundness);

    // 1) Shadow, slightly below the bubble.
    if (style.shadow) {
        const QImage &shadow = shadowFor(rect.size(), radius);
        const int offset = std::max(2, qRound(rect.height() * 0.03));
        painter.drawImage(rect.topLeft() + QPoint(-m_shadowPadding, -m_shadowPadding + offset), shadow);
    }

    // 2) Camera, clipped to the shape with an *anti-aliased* alpha mask. (QPainter's clip
    //    paths are not anti-aliased, which would give the circle jagged edges.)
    QImage bubble(rect.size(), QImage::Format_ARGB32_Premultiplied);
    bubble.fill(Qt::transparent);
    {
        QPainter bp(&bubble);
        bp.setRenderHint(QPainter::Antialiasing);
        bp.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF local(QPointF(0.0, 0.0), QSizeF(rect.size()));
        bp.fillRect(local, kCameraPlaceholder);
        drawCamera(bp, local, style.mirrored);

        // DestinationIn keeps each pixel's colour scaled by the mask's alpha. It only affects
        // pixels the mask *covers*, so the mask must be a full-size image (transparent outside
        // the shape), not just the shape's path — otherwise the corners would survive.
        bp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        bp.drawImage(0, 0, maskFor(rect.size(), radius));

        // 3) Border, drawn inside the shape (like a QML Rectangle border).
        const double border = style.borderPixels(canvasWidth, canvasHeight);
        if (border >= 0.5) {
            bp.setCompositionMode(QPainter::CompositionMode_SourceOver);
            QPen pen(style.borderColor, border);
            pen.setJoinStyle(Qt::RoundJoin);
            bp.setPen(pen);
            bp.setBrush(Qt::NoBrush);
            const double half = border / 2.0;
            const double innerRadius = std::max(0.0, radius - half);
            bp.drawRoundedRect(local.adjusted(half, half, -half, -half), innerRadius, innerRadius);
        }
    }
    painter.drawImage(rect.topLeft(), bubble);
}

const QImage &FrameCompositor::maskFor(const QSize &bubbleSize, double radius) {
    if (bubbleSize == m_maskKeySize && std::abs(radius - m_maskKeyRadius) < 0.01 && !m_mask.isNull()) {
        return m_mask;
    }
    m_mask = QImage(bubbleSize, QImage::Format_ARGB32_Premultiplied);
    m_mask.fill(Qt::transparent);
    {
        QPainter p(&m_mask);
        p.setRenderHint(QPainter::Antialiasing);  // soft 1-px edge instead of jagged steps
        QPainterPath shape;
        shape.addRoundedRect(QRectF(QPointF(0.0, 0.0), QSizeF(bubbleSize)), radius, radius);
        p.fillPath(shape, Qt::black);
    }
    m_maskKeySize = bubbleSize;
    m_maskKeyRadius = radius;
    return m_mask;
}

const QImage &FrameCompositor::shadowFor(const QSize &bubbleSize, double radius) {
    if (bubbleSize == m_shadowKeySize && std::abs(radius - m_shadowKeyRadius) < 0.01 && !m_shadow.isNull()) {
        return m_shadow;
    }
    const int blur = std::max(2, qRound(std::min(bubbleSize.width(), bubbleSize.height()) * 0.06));
    m_shadowPadding = blur * 3;  // two blur passes spread ~2x the radius; keep a margin

    m_shadow = QImage(bubbleSize + QSize(m_shadowPadding * 2, m_shadowPadding * 2), QImage::Format_ARGB32_Premultiplied);
    m_shadow.fill(Qt::transparent);
    {
        QPainter p(&m_shadow);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath shape;
        shape.addRoundedRect(QRectF(m_shadowPadding, m_shadowPadding, bubbleSize.width(), bubbleSize.height()), radius,
                             radius);
        p.fillPath(shape, QColor(0, 0, 0, kShadowAlpha));
    }
    for (int pass = 0; pass < 2; pass++) {
        boxBlurAlpha(m_shadow, blur);
    }
    m_shadowKeySize = bubbleSize;
    m_shadowKeyRadius = radius;
    return m_shadow;
}

QVideoFrame FrameCompositor::toNv12Frame(const QImage &canvas, int framesPerSecond) {
    if (canvas.isNull()) {
        return {};
    }
    const QImage rgb = (canvas.format() == QImage::Format_RGB32 || canvas.format() == QImage::Format_ARGB32)
                           ? canvas
                           : canvas.convertToFormat(QImage::Format_RGB32);

    QVideoFrameFormat format(rgb.size(), QVideoFrameFormat::Format_NV12);
    format.setColorSpace(QVideoFrameFormat::ColorSpace_BT709);
    format.setColorRange(QVideoFrameFormat::ColorRange_Video);
    format.setStreamFrameRate(framesPerSecond);

    QVideoFrame frame(format);
    if (!frame.map(QVideoFrame::WriteOnly)) {
        return {};
    }
    core::convertXrgbToNv12(rgb.constBits(), static_cast<int>(rgb.bytesPerLine()), rgb.width(), rgb.height(),
                            frame.bits(0), frame.bytesPerLine(0), frame.bits(1), frame.bytesPerLine(1));
    frame.unmap();
    return frame;
}

void FrameCompositor::start() {
    m_clock.start();
    emit clockStarted(m_clock.msecsSinceReference());
    m_pausedNs = 0;
    m_pauseStartedNs = -1;
    m_lastTimestampUs = -1;
    m_running = true;
    m_timer->start(std::max(1, 1000 / m_config.framesPerSecond));
    tick();  // first frame right away
}

void FrameCompositor::pause() {
    if (!m_running || isPaused()) {
        return;
    }
    m_pauseStartedNs = m_clock.nsecsElapsed();
    m_timer->stop();
}

void FrameCompositor::resume() {
    if (!m_running || !isPaused()) {
        return;
    }
    m_pausedNs += m_clock.nsecsElapsed() - m_pauseStartedNs;
    m_pauseStartedNs = -1;
    m_timer->start();
    tick();
}

void FrameCompositor::stop() {
    m_timer->stop();
    m_running = false;
    m_pauseStartedNs = -1;
}

qint64 FrameCompositor::activeMicroseconds() const {
    if (!m_clock.isValid()) {
        return 0;
    }
    qint64 active = m_clock.nsecsElapsed() - m_pausedNs;
    if (isPaused()) {
        active -= m_clock.nsecsElapsed() - m_pauseStartedNs;
    }
    return active / 1000;
}

void FrameCompositor::tick() {
    if (!m_running || isPaused() || m_config.canvasSize.isEmpty()) {
        return;
    }
    const qint64 timestamp = activeMicroseconds();
    if (timestamp <= m_lastTimestampUs) {
        return;  // timestamps must strictly increase
    }
    QVideoFrame frame = toNv12Frame(renderCanvas(), m_config.framesPerSecond);
    if (!frame.isValid()) {
        return;
    }
    frame.setStartTime(timestamp);
    frame.setEndTime(timestamp + 1'000'000 / m_config.framesPerSecond);
    m_lastTimestampUs = timestamp;
    emit frameReady(frame);
}
