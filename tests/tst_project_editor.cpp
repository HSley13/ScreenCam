// ProjectEditor: the bridge between the editor UI and the project file.
#include "app/ProjectEditor.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TestProjectEditor : public QObject {
    Q_OBJECT

private:
    // A 30 s project with one segment, saved into `dir`.
    static void writeProject(const QTemporaryDir &dir) {
        Project project;
        project.folder = dir.path();
        project.durationMs = 30000;
        project.canvasSize = QSize(1920, 1080);
        project.cameraOffsetMs = 25;
        project.segments.push_back(ProjectSegment{});
        project.segments.front().style.center = {0.2, 0.8};
        QVERIFY(project.save());
    }

private slots:
    void opensFolderUrlOrJson() {
        QTemporaryDir dir;
        writeProject(dir);
        ProjectEditor editor;
        QVERIFY(editor.open(QUrl::fromLocalFile(dir.path()).toString()));
        QCOMPARE(editor.segmentCount(), 1);
        QCOMPARE(editor.cameraOffsetMs(), qint64(25));
        QCOMPARE(editor.style()->centerX(), 0.2);
        QVERIFY(editor.open(dir.filePath("project.json")));
        QVERIFY(!editor.open(dir.filePath("missing")));
        QVERIFY(!editor.errorText().isEmpty());
    }

    void editingTheStyleChangesOnlyTheCurrentSegment() {
        QTemporaryDir dir;
        writeProject(dir);
        ProjectEditor editor;
        QVERIFY(editor.open(dir.path()));

        QCOMPARE(editor.splitAt(10000), 1);
        QCOMPARE(editor.currentIndex(), 1);  // the part after the cut is current
        editor.style()->moveCenter(0.8, 0.2, 1920, 1080);
        QVERIFY(editor.dirty());

        editor.selectAt(5000);  // back to segment 0: its own, unchanged position
        QCOMPARE(editor.currentIndex(), 0);
        QCOMPARE(editor.style()->centerX(), 0.2);

        editor.selectAt(15000);
        QVERIFY(qAbs(editor.style()->centerX() - 0.8) < 1e-9);
    }

    void selectingASegmentIsNotAnEdit() {
        QTemporaryDir dir;
        writeProject(dir);
        ProjectEditor editor;
        QVERIFY(editor.open(dir.path()));
        editor.splitAt(10000);
        QVERIFY(editor.save());
        QVERIFY(!editor.dirty());
        editor.selectAt(1000);
        editor.selectAt(20000);
        QVERIFY(!editor.dirty());  // loading a segment's look into `style` must not mark dirty
    }

    void autosavesAfterAChange() {
        QTemporaryDir dir;
        writeProject(dir);
        {
            ProjectEditor editor;
            QVERIFY(editor.open(dir.path()));
            editor.splitAt(12000);
            editor.setCameraVisible(false);
            QTRY_VERIFY_WITH_TIMEOUT(!editor.dirty(), 3000);
        }
        const std::optional<Project> saved = Project::load(dir.path());
        QVERIFY(saved.has_value());
        QCOMPARE(saved->segments.size(), size_t(2));
        QCOMPARE(saved->segments.at(1).cameraVisible, false);
        QCOMPARE(saved->segments.at(0).cameraVisible, true);
    }

    void removeAndApplyToAll() {
        QTemporaryDir dir;
        writeProject(dir);
        ProjectEditor editor;
        QVERIFY(editor.open(dir.path()));
        editor.splitAt(10000);
        editor.splitAt(20000);
        QCOMPARE(editor.segmentCount(), 3);

        editor.style()->setShape(BubbleStyle::Shape::Portrait);  // segment 2
        editor.applyToAll();
        editor.selectAt(0);
        QCOMPARE(editor.style()->shape(), BubbleStyle::Shape::Portrait);

        editor.selectAt(15000);
        QVERIFY(editor.removeCurrent());
        QCOMPARE(editor.segmentCount(), 2);
        QCOMPARE(editor.currentIndex(), 0);  // segment 0 took over 10-20 s
        QCOMPARE(editor.segmentEnd(0), qint64(20000));
        QVERIFY(editor.removeCurrent());
        QVERIFY(!editor.removeCurrent());  // the last segment stays
    }

    void durationNeverCutsOffASegment() {
        QTemporaryDir dir;
        writeProject(dir);
        ProjectEditor editor;
        QVERIFY(editor.open(dir.path()));
        editor.splitAt(20000);
        editor.setDurationMs(20100);  // would leave the last segment 0.1 s: refused
        QCOMPARE(editor.durationMs(), qint64(30000));
        editor.setDurationMs(29500);
        QCOMPARE(editor.segmentEnd(1), qint64(29500));
    }

    void exportsNextToTheProjectAndRemembersIt() {
        if (!ProjectEditor::exportAvailable()) {
            QSKIP("built without FFmpeg: no export");
        }
        const QString sample = qEnvironmentVariable("SCREENCAM_SAMPLE_PROJECT");
        if (sample.isEmpty()) {
            QSKIP("run through ctest (needs the make_sample_project fixture)");
        }
        // Work on a copy: the export lands beside the project folder.
        QTemporaryDir dir;
        const QString folder = dir.filePath("Talk.screencam");
        QVERIFY(QDir().mkpath(folder));
        for (const QString &name : QDir(sample).entryList(QDir::Files)) {
            QVERIFY(QFile::copy(QDir(sample).filePath(name), QDir(folder).filePath(name)));
        }

        ProjectEditor editor;
        QVERIFY(editor.open(folder));
        QCOMPARE(editor.exportPath(), dir.filePath("Talk.mp4"));
        QSignalSpy done(&editor, &ProjectEditor::exportFinished);
        QVERIFY(editor.exportVideo());
        QVERIFY(editor.exporting());
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty() || !editor.errorText().isEmpty(), 60000);
        QVERIFY2(editor.errorText().isEmpty(), qPrintable(editor.errorText()));
        QVERIFY(!editor.exporting());
        QCOMPARE(editor.lastExport(), dir.filePath("Talk.mp4"));
        QVERIFY(QFileInfo(editor.lastExport()).size() > 0);

        // Saved in project.json; a second export replaces the first instead of "Talk 2.mp4".
        editor.close();
        QVERIFY(editor.open(folder));
        QCOMPARE(editor.lastExport(), dir.filePath("Talk.mp4"));
        QCOMPARE(editor.exportPath(), dir.filePath("Talk.mp4"));
    }

    void formatsTimeWithTenths() {
        ProjectEditor editor;
        QCOMPARE(editor.formatTime(65300), QStringLiteral("1:05.3"));
        QCOMPARE(editor.formatTime(0), QStringLiteral("0:00.0"));
        QCOMPARE(editor.formatTime(3723400), QStringLiteral("1:02:03.4"));
    }
};

QTEST_GUILESS_MAIN(TestProjectEditor)
#include "tst_project_editor.moc"
