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

/// What the video frame is made of.
enum class CompositeLayout {
    ScreenWithBubble,  // screen, camera bubble on top
    ScreenOnly,
    CameraOnly,        // camera fills the frame (16:9 or 9:16)
};

/// The mouse pointer to draw over the screen picture (see core/Pointer.hpp for why it's drawn
/// at all). Positions are relative to the *captured* frame, before any region crop.
struct PointerOverlay {
    bool visible = false;
    QPointF tip;            // tip of the arrow, 0..1 of the captured frame
    double height = 0.0;    // arrow height, as a fraction of the captured frame's height
    bool highlight = false; // yellow halo around it, so viewers find it at a glance
    bool arrow = true;      // false: the capture already shows the pointer, add only the halo
};

struct CompositorConfig {
    QSize canvasSize;  // output video size in pixels
    // Part of the screen frame to record, normalized (0..1). The whole frame by default; a
    // smaller rectangle records just a region of the screen.
    QRectF sourceCrop{0.0, 0.0, 1.0, 1.0};
    CompositeLayout layout = CompositeLayout::ScreenWithBubble;
    int framesPerSecond = 30;
    BubbleStyleSnapshot bubble;
};

/// Builds the frames of the video, at a steady frame rate, from the latest screen and camera
/// frames. Lives on its own thread (see RecordingPipeline) so painting never stalls the UI.
///
/// Per tick:  latest QVideoFrames -> QImage -> paint canvas (screen, bubble with shadow,
/// mask and border) -> NV12 (4:2:0, BT.709) -> QVideoFrame stamped with the *real* elapsed
/// recording time (pauses excluded) -> frameReady().
///
/// Why stamp frames ourselves: the encoder places frames on the timeline only by their
/// timestamps. Without them it assumes a perfect frame rate, so any late frame would make
/// the video drift away from the microphone audio.
class FrameCompositor : public QObject {
    Q_OBJECT

public:
    explicit FrameCompositor(QObject *parent = nullptr);

    void configure(const CompositorConfig &config);
    const CompositorConfig &config() const { return m_config; }
    void setBubble(const BubbleStyleSnapshot &bubble);
    void setPointer(const PointerOverlay &pointer) { m_pointer = pointer; }

    // Inputs. Frames are only converted to images when a tick needs them.
    void setScreenFrame(const QVideoFrame &frame);
    void setCameraFrame(const QVideoFrame &frame);
    void setScreenImage(const QImage &image);
    void setCameraImage(const QImage &image);

    /// Paints one canvas from the latest inputs (public for tests).
    QImage renderCanvas();
    /// Converts a canvas to an NV12 video frame (no timestamps).
    static QVideoFrame toNv12Frame(const QImage &canvas, int framesPerSecond);

    void start();
    void pause();
    void resume();
    void stop();
    bool isRunning() const { return m_running; }
    bool isPaused() const { return m_pauseStartedNs >= 0; }

signals:
    void frameReady(const QVideoFrame &frame);
    /// Video time 0, on the machine's monotonic clock (QElapsedTimer::msecsSinceReference):
    /// lets two recordings made in parallel be lined up afterwards.
    void clockStarted(qint64 referenceMs);

private:
    void tick();
    void refreshImages();
    void drawCamera(QPainter &painter, const QRectF &target, bool mirrored) const;
    void drawScreen(QPainter &painter, const QRectF &canvas) const;
    void drawPointer(QPainter &painter, const QRectF &canvas) const;
    /// Where the recorded part of the screen picture lands on the canvas, and which part that
    /// is (normalized crop). Shared by drawScreen() and drawPointer().
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
    qint64 m_pausedNs = 0;         // total time spent paused
    qint64 m_pauseStartedNs = -1;  // >= 0 while paused
    qint64 m_lastTimestampUs = -1;

    // The blurred shadow only changes when the bubble's size/shape does: cache it.
    QImage m_shadow;
    QSize m_shadowKeySize;
    double m_shadowKeyRadius = -1.0;
    int m_shadowPadding = 0;
    // Same for the anti-aliased shape mask.
    QImage m_mask;
    QSize m_maskKeySize;
    double m_maskKeyRadius = -1.0;
};
