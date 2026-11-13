#pragma once

#include "media/BubbleStyleSnapshot.hpp"

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QRectF>
#include <QSize>
#include <QVideoFrame>

class QPainter;
class QTimer;

enum class CompositeLayout {
    ScreenWithBubble,  // screen, camera bubble on top
    ScreenOnly,
    CameraOnly,        // camera fills the frame (16:9 or 9:16)
};

/// Positions are relative to the captured frame, before any region crop.
struct PointerOverlay {
    bool visible = false;
    QPointF tip;            // tip of the arrow, 0..1 of the captured frame
    double height = 0.0;    // arrow height, as a fraction of the captured frame's height
    bool highlight = false; // yellow halo
    bool arrow = true;      // false: the capture already shows the pointer, add only the halo
};

struct CompositorConfig {
    QSize canvasSize;  // output video size in pixels
    // Normalized part of the screen frame to record (a region, or all of it).
    QRectF sourceCrop{0.0, 0.0, 1.0, 1.0};
    CompositeLayout layout = CompositeLayout::ScreenWithBubble;
    int framesPerSecond = 30;
    BubbleStyleSnapshot bubble;
};

/// Paints the video frames at a steady rate from the latest screen and camera frames, on its
/// own thread. Each frame is converted to NV12 and stamped with the real elapsed recording
/// time (pauses excluded): without timestamps the encoder assumes a perfect frame rate, and
/// any late frame would make the video drift away from the audio.
class FrameCompositor : public QObject {
    Q_OBJECT

public:
    explicit FrameCompositor(QObject *parent = nullptr);

    void configure(const CompositorConfig &config);
    const CompositorConfig &config() const { return m_config; }
    void setBubble(const BubbleStyleSnapshot &bubble);
    void setPointer(const PointerOverlay &pointer) { m_pointer = pointer; }

    // Frames are only converted to images when a tick needs them.
    void setScreenFrame(const QVideoFrame &frame);
    void setCameraFrame(const QVideoFrame &frame);
    void setScreenImage(const QImage &image);
    void setCameraImage(const QImage &image);

    QImage renderCanvas();  // public for tests
    static QVideoFrame toNv12Frame(const QImage &canvas, int framesPerSecond);

    void start();
    void pause();
    void resume();
    void stop();
    bool isRunning() const { return m_running; }
    bool isPaused() const { return m_pauseStartedNs >= 0; }

signals:
    void frameReady(const QVideoFrame &frame);
    /// Video time 0 on the monotonic clock, so parallel recordings can be lined up afterwards.
    void clockStarted(qint64 referenceMs);

private:
    void tick();
    void refreshImages();
    void drawCamera(QPainter &painter, const QRectF &target, bool mirrored) const;
    void drawScreen(QPainter &painter, const QRectF &canvas) const;
    void drawPointer(QPainter &painter, const QRectF &canvas) const;
    /// Where the recorded part of the screen lands on the canvas, and which part (`crop`).
    QRectF screenTargetRect(const QRectF &canvas, QRectF *crop) const;
    void drawBubble(QPainter &painter);
    const QImage &shadowFor(const QSize &bubbleSize, double radius);
    const QImage &maskFor(const QSize &bubbleSize, double radius);
    qint64 activeMicroseconds() const;

    CompositorConfig m_config;

    QVideoFrame m_screenFrame;
    QVideoFrame m_cameraFrame;
    bool m_screenDirty = false;
    bool m_cameraDirty = false;
    QImage m_screenImage;
    QImage m_cameraImage;
    PointerOverlay m_pointer;

    QTimer *m_timer = nullptr;
    bool m_running = false;
    QElapsedTimer m_clock;
    qint64 m_pausedNs = 0;
    qint64 m_pauseStartedNs = -1;  // >= 0 while paused
    qint64 m_lastTimestampUs = -1;

    // Shadow and mask only change with the bubble's size and shape.
    QImage m_shadow;
    QSize m_shadowKeySize;
    double m_shadowKeyRadius = -1.0;
    int m_shadowPadding = 0;

    QImage m_mask;
    QSize m_maskKeySize;
    double m_maskKeyRadius = -1.0;
};
