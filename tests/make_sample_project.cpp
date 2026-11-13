// Writes a small real project from synthetic pictures (a CTest fixture, and handy by hand):
//   make_sample_project /tmp/Sample.screencam [seconds]
#include "media/Project.hpp"
#include "media/RecordingPipeline.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QGuiApplication>
#include <QPainter>
#include <QTimer>

namespace {

// A screen that visibly changes over time: a hue sweep with a big frame counter.
QImage screenPicture(int tick) {
    QImage image(1280, 720, QImage::Format_RGB32);
    image.fill(QColor::fromHsv((tick * 4) % 360, 120, 90));
    QPainter painter(&image);
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(120);
    painter.setFont(font);
    painter.drawText(image.rect(), Qt::AlignCenter, QString::number(tick));
    return image;
}

// A "camera": a face-coloured circle on a dark background that moves a little.
QImage cameraPicture(int tick) {
    QImage image(640, 480, QImage::Format_RGB32);
    image.fill(QColor(30, 40, 60));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(230, 180, 140));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(320 + 40 * std::sin(tick / 6.0), 240), 120, 150);
    return image;
}

}  // namespace

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);  // QPainter text needs fonts, i.e. a GUI application
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 2) {
        qCritical("usage: make_sample_project <folder.screencam> [seconds]");
        return 2;
    }
    const QString folder = QDir(args.at(1)).absolutePath();
    const int seconds = args.size() > 2 ? args.at(2).toInt() : 3;
    QDir().mkpath(folder);

    RecordingPipeline screen;
    RecordingPipeline camera;
    RecordingPipeline::Request screenRequest;
    screenRequest.outputPath = QDir(folder).filePath("screen.mp4");
    screenRequest.compositor.canvasSize = QSize(1280, 720);
    screenRequest.compositor.layout = CompositeLayout::ScreenOnly;
    RecordingPipeline::Request cameraRequest;
    cameraRequest.outputPath = QDir(folder).filePath("camera.mp4");
    cameraRequest.compositor.canvasSize = QSize(640, 480);
    cameraRequest.compositor.layout = CompositeLayout::CameraOnly;
    cameraRequest.compositor.bubble.mirrored = false;

    qint64 screenClock = -1;
    qint64 cameraClock = -1;
    QObject::connect(&screen, &RecordingPipeline::clockStarted, [&screenClock](qint64 ms) { screenClock = ms; });
    QObject::connect(&camera, &RecordingPipeline::clockStarted, [&cameraClock](qint64 ms) { cameraClock = ms; });
    int done = 0;
    QString error;
    QEventLoop loop;
    for (RecordingPipeline *pipeline : {&screen, &camera}) {
        QObject::connect(pipeline, &RecordingPipeline::finished, &loop, [&done, &loop] {
            done++;
            if (done == 2) {
                loop.quit();
            }
        });
        QObject::connect(pipeline, &RecordingPipeline::failed, &loop, [&error, &loop](const QString &message) {
            error = message;
            loop.quit();
        });
    }

    int tick = 0;
    screen.pushScreenImage(screenPicture(0));
    camera.pushCameraImage(cameraPicture(0));
    if (!screen.start(screenRequest) || !camera.start(cameraRequest)) {
        qCritical("couldn't start recording (is the FFmpeg multimedia backend installed?)");
        return 1;
    }
    QTimer feeder;
    QObject::connect(&feeder, &QTimer::timeout, [&] {
        tick++;
        screen.pushScreenImage(screenPicture(tick));
        camera.pushCameraImage(cameraPicture(tick));
    });
    feeder.start(100);
    QTimer::singleShot(seconds * 1000, [&] {
        feeder.stop();
        screen.stop();
        camera.stop();
    });
    loop.exec();
    if (!error.isEmpty()) {
        qCritical().noquote() << "recording failed:" << error;
        return 1;
    }

    Project project;
    project.folder = folder;
    project.durationMs = seconds * 1000;
    project.cameraOffsetMs = (screenClock >= 0 && cameraClock >= 0) ? cameraClock - screenClock : 0;
    project.canvasSize = QSize(1280, 720);
    project.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    project.segments.push_back(ProjectSegment{});
    project.splitAt(project.durationMs / 2);  // second half: bubble in the top-right corner
    project.segments.at(1).style.center = {0.86, 0.2};
    project.segments.at(1).style.shape = screencam::core::BubbleShape::Squircle;
    if (!project.save(&error)) {
        qCritical().noquote() << error;
        return 1;
    }
    qInfo().noquote() << "wrote" << folder;
    return 0;
}
