// End-to-end: synthetic screen + camera images -> compositor -> encoder -> MP4 file, then
// open the file with QMediaPlayer and check duration, size and decoded pixel colours.
#include "media/RecordingPipeline.hpp"

#include <QFileInfo>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVideoSink>
#include <QtTest>

#include <cstdlib>

namespace {

QImage solid(const QSize &size, const QColor &color) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);
    return image;
}

bool roughly(const QColor &a, const QColor &b, int tolerance = 45) {
    return std::abs(a.red() - b.red()) <= tolerance && std::abs(a.green() - b.green()) <= tolerance &&
           std::abs(a.blue() - b.blue()) <= tolerance;
}

}  // namespace

class TestPipeline : public QObject {
    Q_OBJECT

private slots:
    void recordsAPlayableFileWithPauseExcluded() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Set SCREENCAM_KEEP_TEST_OUTPUT=1 to keep the recorded file for inspection.
        dir.setAutoRemove(qEnvironmentVariableIsEmpty("SCREENCAM_KEEP_TEST_OUTPUT"));
        const QString path = dir.filePath(QStringLiteral("test.mp4"));

        RecordingPipeline pipeline;
        QSignalSpy finished(&pipeline, &RecordingPipeline::finished);
        QSignalSpy failed(&pipeline, &RecordingPipeline::failed);

        RecordingPipeline::Request request;
        request.outputPath = path;
        request.videoBitRate = 2'000'000;
        request.compositor.canvasSize = QSize(640, 360);
        request.compositor.layout = CompositeLayout::ScreenWithBubble;
        request.compositor.framesPerSecond = 30;
        request.compositor.bubble.center = {0.5, 0.5};
        request.compositor.bubble.size = 0.4;
        request.compositor.bubble.shadow = false;
        request.compositor.bubble.borderWidth = 0.0;

        pipeline.pushScreenImage(solid(QSize(1280, 720), QColor(200, 40, 40)));
        pipeline.pushCameraImage(solid(QSize(1280, 720), QColor(40, 200, 40)));
        QVERIFY(pipeline.start(request));

        QTest::qWait(1000);
        pipeline.pause();
        QTest::qWait(600);  // paused time must not end up in the video
        pipeline.resume();
        QTest::qWait(700);
        pipeline.stop();

        QVERIFY2(finished.wait(15000) || !finished.isEmpty(),
                 failed.isEmpty() ? "no result" : qPrintable(failed.first().first().toString()));
        QCOMPARE(failed.count(), 0);
        QVERIFY(QFileInfo(path).size() > 0);
        qInfo() << "recorded" << path << "dropped frames:" << pipeline.droppedFrames();

        // --- Inspect the file ---------------------------------------------------------
        QMediaPlayer player;
        QVideoSink sink;
        player.setVideoSink(&sink);
        QVideoFrame firstFrame;
        connect(&sink, &QVideoSink::videoFrameChanged, this, [&firstFrame](const QVideoFrame &frame) {
            if (!firstFrame.isValid() && frame.isValid()) {
                firstFrame = frame;
            }
        });
        player.setSource(QUrl::fromLocalFile(path));
        QTRY_VERIFY_WITH_TIMEOUT(player.mediaStatus() == QMediaPlayer::LoadedMedia, 10000);

        const qint64 duration = player.duration();
        qInfo() << "duration (ms):" << duration;
        // 1.0 s + 0.7 s of active recording; the 0.6 s pause is excluded.
        QVERIFY2(duration > 1300 && duration < 2200, qPrintable(QString::number(duration)));
        QCOMPARE(player.metaData().value(QMediaMetaData::Resolution).toSize(), QSize(640, 360));

        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(firstFrame.isValid(), 10000);
        player.stop();
        const QImage image = firstFrame.toImage();
        QCOMPARE(image.size(), QSize(640, 360));
        QVERIFY2(roughly(image.pixelColor(320, 180), QColor(40, 200, 40)),
                 qPrintable(image.pixelColor(320, 180).name()));  // bubble centre: camera
        QVERIFY2(roughly(image.pixelColor(30, 30), QColor(200, 40, 40)),
                 qPrintable(image.pixelColor(30, 30).name()));    // corner: screen
    }

    void rejectsAnEmptyCanvas() {
        RecordingPipeline pipeline;
        QSignalSpy failed(&pipeline, &RecordingPipeline::failed);
        RecordingPipeline::Request request;
        request.outputPath = QStringLiteral("unused.mp4");
        QVERIFY(!pipeline.start(request));
        QCOMPARE(failed.count(), 1);
    }
};

QTEST_MAIN(TestPipeline)
#include "tst_pipeline.moc"
