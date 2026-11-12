// Pixel-level tests of the compositor: what we paint is what gets encoded.
#include "media/FrameCompositor.hpp"

#include <QSignalSpy>
#include <QtTest>

#include <cstdlib>

namespace {

QImage solid(const QSize &size, const QColor &color) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);
    return image;
}

bool closeTo(const QColor &a, const QColor &b, int tolerance = 8) {
    return std::abs(a.red() - b.red()) <= tolerance && std::abs(a.green() - b.green()) <= tolerance &&
           std::abs(a.blue() - b.blue()) <= tolerance;
}

/// 640x360 canvas, plain circle of 40% height in the middle, no border/shadow/mirroring.
CompositorConfig baseConfig(CompositeLayout layout) {
    CompositorConfig config;
    config.canvasSize = QSize(640, 360);
    config.layout = layout;
    config.framesPerSecond = 30;
    config.bubble.shape = screencam::core::BubbleShape::Circle;
    config.bubble.size = 0.4;  // diameter 144 px
    config.bubble.center = {0.5, 0.5};
    config.bubble.shadow = false;
    config.bubble.borderWidth = 0.0;
    config.bubble.mirrored = false;
    return config;
}

}  // namespace

class TestCompositor : public QObject {
    Q_OBJECT

private slots:
    void screenFillsTheCanvas() {
        FrameCompositor compositor;
        compositor.configure(baseConfig(CompositeLayout::ScreenOnly));
        compositor.setScreenImage(solid(QSize(1280, 720), Qt::red));
        const QImage canvas = compositor.renderCanvas();
        QCOMPARE(canvas.size(), QSize(640, 360));
        QVERIFY(closeTo(canvas.pixelColor(0, 0), Qt::red));
        QVERIFY(closeTo(canvas.pixelColor(639, 359), Qt::red));
    }

    void bubbleIsACircleOverTheScreen() {
        FrameCompositor compositor;
        compositor.configure(baseConfig(CompositeLayout::ScreenWithBubble));
        compositor.setScreenImage(solid(QSize(1280, 720), Qt::red));
        compositor.setCameraImage(solid(QSize(1280, 720), Qt::green));
        const QImage canvas = compositor.renderCanvas();
        // Centre of the bubble shows the camera...
        QVERIFY(closeTo(canvas.pixelColor(320, 180), Qt::green));
        // ...a corner of its bounding box is outside the circle, so it shows the screen.
        QVERIFY(closeTo(canvas.pixelColor(320 - 72 + 3, 180 - 72 + 3), Qt::red));
        // Well outside the bubble: screen.
        QVERIFY(closeTo(canvas.pixelColor(20, 20), Qt::red));
    }

    void borderIsDrawnInsideTheShape() {
        CompositorConfig config = baseConfig(CompositeLayout::ScreenWithBubble);
        config.bubble.borderWidth = 15.0;  // 15 * 360 / 1080 = 5 px on this canvas
        config.bubble.borderColor = Qt::white;
        FrameCompositor compositor;
        compositor.configure(config);
        compositor.setScreenImage(solid(QSize(640, 360), Qt::red));
        compositor.setCameraImage(solid(QSize(640, 360), Qt::green));
        const QImage canvas = compositor.renderCanvas();
        QVERIFY(closeTo(canvas.pixelColor(320, 180 - 72 + 2), Qt::white, 30));  // in the ring
        QVERIFY(closeTo(canvas.pixelColor(320, 180), Qt::green));               // camera inside
    }

    void mirroringFlipsTheCamera() {
        // Camera: left half blue, right half yellow.
        QImage camera(640, 360, QImage::Format_RGB32);
        camera.fill(Qt::yellow);
        for (int y = 0; y < camera.height(); y++) {
            for (int x = 0; x < camera.width() / 2; x++) {
                camera.setPixelColor(x, y, Qt::blue);
            }
        }
        CompositorConfig config = baseConfig(CompositeLayout::CameraOnly);
        FrameCompositor compositor;
        compositor.setCameraImage(camera);

        compositor.configure(config);
        QVERIFY(closeTo(compositor.renderCanvas().pixelColor(100, 180), Qt::blue));

        config.bubble.mirrored = true;
        compositor.configure(config);
        QVERIFY(closeTo(compositor.renderCanvas().pixelColor(100, 180), Qt::yellow));
    }

    void cameraOnlyCropsInsteadOfStretching() {
        // Portrait output from a landscape camera: only the middle strip may appear.
        QImage camera(640, 360, QImage::Format_RGB32);
        camera.fill(Qt::magenta);
        for (int y = 0; y < camera.height(); y++) {
            for (int x = 220; x < 420; x++) {
                camera.setPixelColor(x, y, Qt::cyan);  // centre 200 px
            }
        }
        CompositorConfig config = baseConfig(CompositeLayout::CameraOnly);
        config.canvasSize = QSize(202, 360);  // 9:16-ish: crop width = 360 * 202/360 = 202 px
        FrameCompositor compositor;
        compositor.configure(config);
        compositor.setCameraImage(camera);
        const QImage canvas = compositor.renderCanvas();
        QVERIFY(closeTo(canvas.pixelColor(101, 180), Qt::cyan));
        QVERIFY(closeTo(canvas.pixelColor(5, 180), Qt::cyan, 60));  // edges still from the strip
    }

    void nv12FrameHasTheRightFormat() {
        const QVideoFrame frame = FrameCompositor::toNv12Frame(solid(QSize(64, 32), Qt::white), 30);
        QVERIFY(frame.isValid());
        QCOMPARE(frame.pixelFormat(), QVideoFrameFormat::Format_NV12);
        QCOMPARE(frame.size(), QSize(64, 32));
        QVideoFrame copy = frame;
        QVERIFY(copy.map(QVideoFrame::ReadOnly));
        QCOMPARE(int(copy.bits(0)[0]), 235);  // white in BT.709 video range
        QCOMPARE(int(copy.bits(1)[0]), 128);  // no colour
        copy.unmap();
    }

    void timestampsIncreaseAndSkipPauses() {
        FrameCompositor compositor;
        compositor.configure(baseConfig(CompositeLayout::ScreenOnly));
        compositor.setScreenImage(solid(QSize(640, 360), Qt::darkGray));
        QSignalSpy spy(&compositor, &FrameCompositor::frameReady);

        compositor.start();
        QTest::qWait(200);
        compositor.pause();
        QTest::qWait(300);  // must not appear in the timestamps
        compositor.resume();
        QTest::qWait(200);
        compositor.stop();

        QVERIFY(spy.count() >= 6);
        qint64 previous = -1;
        for (const QList<QVariant> &args : spy) {
            const auto frame = args.at(0).value<QVideoFrame>();
            QVERIFY(frame.startTime() > previous);
            QVERIFY(frame.endTime() > frame.startTime());
            previous = frame.startTime();
        }
        // ~400 ms of active time; the 300 ms pause is excluded (generous bounds for slow CI).
        QVERIFY2(previous > 250'000, qPrintable(QString::number(previous)));
        QVERIFY2(previous < 600'000, qPrintable(QString::number(previous)));
    }
};

QTEST_MAIN(TestCompositor)
#include "tst_compositor.moc"
