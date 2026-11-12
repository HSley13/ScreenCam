// Export end to end: the sample project (screen + camera + two segments) -> MP4, then decode
// the result and check that the bubble is where each segment says.
#include "export/ProjectExporter.hpp"
#include "export/VideoFileReader.hpp"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <cstdlib>

namespace {

// make_sample_project's camera picture: a skin-coloured face on a dark blue background.
bool looksLikeFace(const QColor &color) {
    return color.red() > 170 && color.green() > 120 && color.blue() < 190 && color.red() > color.blue() + 40;
}

}  // namespace

class TestExport : public QObject {
    Q_OBJECT

private:
    Project m_project;

private slots:
    void initTestCase() {
        const QString folder = qEnvironmentVariable("SCREENCAM_SAMPLE_PROJECT");
        QVERIFY2(!folder.isEmpty(), "run through ctest (the make_sample_project fixture sets this up)");
        QString error;
        const std::optional<Project> project = Project::load(folder, &error);
        QVERIFY2(project.has_value(), qPrintable(error));
        m_project = *project;
        qInfo() << "encoder:" << ProjectExporter::encoderNameFor(m_project.canvasSize.width(),
                                                                 m_project.canvasSize.height());
    }

    void exportsTheBubbleWhereEachSegmentPutsIt() {
        QTemporaryDir dir;
        dir.setAutoRemove(qEnvironmentVariableIsEmpty("SCREENCAM_KEEP_TEST_OUTPUT"));
        const QString output = dir.filePath("export.mp4");
        double lastProgress = 0.0;
        const QString error = ProjectExporter::exportProject(m_project, output, [&lastProgress](double ratio) {
            lastProgress = ratio;
            return true;
        });
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(lastProgress, 1.0);
        QVERIFY(QFileInfo::exists(output));
        QVERIFY(!QFileInfo::exists(output + ".part"));
        qInfo() << "exported" << output << QFileInfo(output).size() << "bytes";

        VideoFileReader reader;
        QString readError;
        QVERIFY2(reader.open(output, &readError), qPrintable(readError));

        const QSize size = m_project.canvasSize;
        const auto pixel = [&size](const QImage &image, double x, double y) {
            return image.pixelColor(static_cast<int>(x * size.width()), static_cast<int>(y * size.height()));
        };
        // Segment 1 (first half): bubble bottom-left at (0.12, 0.84).
        const QImage early = reader.imageAt(500);
        QCOMPARE(early.size(), size);
        QVERIFY2(looksLikeFace(pixel(early, 0.12, 0.84)), qPrintable(pixel(early, 0.12, 0.84).name()));
        QVERIFY2(!looksLikeFace(pixel(early, 0.86, 0.2)), qPrintable(pixel(early, 0.86, 0.2).name()));
        // Segment 2 (second half): moved to the top right.
        const QImage late = reader.imageAt(2500);
        QVERIFY2(looksLikeFace(pixel(late, 0.86, 0.2)), qPrintable(pixel(late, 0.86, 0.2).name()));
        QVERIFY2(!looksLikeFace(pixel(late, 0.12, 0.84)), qPrintable(pixel(late, 0.12, 0.84).name()));
    }

    void hiddenCameraLeavesTheScreenAlone() {
        Project project = m_project;
        for (ProjectSegment &segment : project.segments) {
            segment.cameraVisible = false;
        }
        QTemporaryDir dir;
        const QString output = dir.filePath("no-camera.mp4");
        QVERIFY(ProjectExporter::exportProject(project, output).isEmpty());
        VideoFileReader reader;
        QString error;
        QVERIFY(reader.open(output, &error));
        const QImage image = reader.imageAt(500);
        QVERIFY(!looksLikeFace(image.pixelColor(static_cast<int>(0.12 * image.width()),
                                                static_cast<int>(0.84 * image.height()))));
    }

    void cancelingLeavesNoFile() {
        QTemporaryDir dir;
        const QString output = dir.filePath("canceled.mp4");
        const QString error = ProjectExporter::exportProject(m_project, output, [](double ratio) {
            return ratio < 0.3;  // stop a third of the way in
        });
        QCOMPARE(error, QStringLiteral("canceled"));
        QVERIFY(!QFileInfo::exists(output));
        QVERIFY(!QFileInfo::exists(output + ".part"));
    }

    void workerThreadReportsProgressAndFinishes() {
        QTemporaryDir dir;
        const QString output = dir.filePath("threaded.mp4");
        ProjectExporter exporter;
        QSignalSpy progress(&exporter, &ProjectExporter::progress);
        QSignalSpy finished(&exporter, &ProjectExporter::finished);
        QSignalSpy failed(&exporter, &ProjectExporter::failed);
        QVERIFY(exporter.start(m_project, output));
        QVERIFY(exporter.isRunning());
        QVERIFY(!exporter.start(m_project, output));  // one at a time
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty() || !failed.isEmpty(), 60000);
        QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.first().first().toString()));
        QVERIFY(progress.count() > 1);
        QVERIFY(!exporter.isRunning());
    }
};

QTEST_GUILESS_MAIN(TestExport)
#include "tst_export.moc"
