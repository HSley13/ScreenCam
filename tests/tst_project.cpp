// Project model: timeline editing and project.json round trip.
#include "media/Project.hpp"

#include <QTemporaryDir>
#include <QtTest>

class TestProject : public QObject {
    Q_OBJECT

private:
    static Project makeProject() {
        Project project;
        project.durationMs = 30000;
        project.segments.push_back(ProjectSegment{});
        project.segments.front().style.center = {0.2, 0.8};
        return project;
    }

private slots:
    void splittingCopiesTheCurrentLayout() {
        Project project = makeProject();
        QCOMPARE(project.splitAt(10000), 1);
        QCOMPARE(project.segments.size(), size_t(2));
        QCOMPARE(project.segments.at(1).startMs, qint64(10000));
        QCOMPARE(project.segments.at(1).style.center.x, 0.2);  // same layout to start from

        QCOMPARE(project.splitAt(5000), 1);  // inserted before the 10 s cut: list stays sorted
        QCOMPARE(project.segments.at(2).startMs, qint64(10000));
        QCOMPARE(project.splitAt(5200), -1);  // too close to the 5 s cut
        QCOMPARE(project.segmentAt(7000), 1);
    }

    void removingASegmentGivesItsTimeToANeighbour() {
        Project project = makeProject();
        project.splitAt(10000);
        project.splitAt(20000);
        QVERIFY(project.removeSegment(1));  // 10-20 s now belongs to segment 0
        QCOMPARE(project.segmentAt(15000), 0);

        QVERIFY(project.removeSegment(0));  // first removed: the next one starts at 0
        QCOMPARE(project.segments.front().startMs, qint64(0));
        QVERIFY(!project.removeSegment(0));  // never remove the last segment
    }

    void savesAndLoadsProjectJson() {
        QTemporaryDir dir;
        Project project = makeProject();
        project.folder = dir.path();
        project.cameraOffsetMs = 42;
        project.canvasSize = QSize(1920, 1080);
        project.splitAt(12000);
        project.segments.at(1).cameraVisible = false;
        project.segments.at(1).style.shape = screencam::core::BubbleShape::Portrait;
        project.segments.at(1).style.borderColor = QColor("#ff7c5cff");
        QString error;
        QVERIFY2(project.save(&error), qPrintable(error));

        const std::optional<Project> loaded = Project::load(dir.path(), &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->cameraOffsetMs, qint64(42));
        QCOMPARE(loaded->canvasSize, QSize(1920, 1080));
        QCOMPARE(loaded->segments.size(), size_t(2));
        QCOMPARE(loaded->segments.at(1).startMs, qint64(12000));
        QCOMPARE(loaded->segments.at(1).cameraVisible, false);
        QVERIFY(loaded->segments.at(1).style.shape == screencam::core::BubbleShape::Portrait);
        QCOMPARE(loaded->segments.at(1).style.borderColor, QColor("#ff7c5cff"));
        QCOMPARE(loaded->segments.at(0).style.center.x, 0.2);
    }

    void reportsAMissingProject() {
        QTemporaryDir dir;
        QString error;
        QVERIFY(!Project::load(dir.path(), &error).has_value());
        QVERIFY(error.contains("project.json"));
    }
};

QTEST_MAIN(TestProject)
#include "tst_project.moc"
