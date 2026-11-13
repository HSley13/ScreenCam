// The "Recordings" list: projects and videos in the output folder, exports grouped with
// their project, newest first.
#include "media/Project.hpp"
#include "media/RecordingIndex.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestRecordingIndex : public QObject {
    Q_OBJECT

private:
    static void touch(const QString &path, const QDateTime &when) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
        file.close();
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(when, QFileDevice::FileModificationTime));
    }

    static void makeProject(const QString &folder, qint64 durationMs, const QString &lastExport,
                            const QDateTime &when) {
        QVERIFY(QDir().mkpath(folder));
        Project project;
        project.folder = folder;
        project.durationMs = durationMs;
        project.lastExport = lastExport;
        project.segments.push_back(ProjectSegment{});
        QVERIFY(project.save());
        QFile json(project.jsonPath());
        QVERIFY(json.open(QIODevice::ReadWrite));
        QVERIFY(json.setFileTime(when, QFileDevice::FileModificationTime));
    }

private slots:
    void listsProjectsAndVideosNewestFirst() {
        QTemporaryDir dir;
        const QDateTime now = QDateTime::currentDateTime();
        const QString talkExport = dir.filePath("Talk.mp4");
        makeProject(dir.filePath("Talk.screencam"), 42000, talkExport, now.addSecs(-60));
        touch(talkExport, now.addSecs(-30));                        // the export of "Talk"
        makeProject(dir.filePath("Demo.screencam"), 5000, QString(), now.addSecs(-10));
        touch(dir.filePath("Screen only.mp4"), now.addSecs(-3600));  // a direct recording
        QVERIFY(QDir().mkpath(dir.filePath("Broken.screencam")));    // no project.json: skipped
        touch(dir.filePath("notes.txt"), now);                       // not a recording

        const QList<RecordingEntry> entries = RecordingIndex::scan(dir.path());
        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries.at(0).name, QStringLiteral("Demo"));
        QVERIFY(entries.at(0).isProject);
        QCOMPARE(entries.at(0).durationMs, qint64(5000));
        QVERIFY(entries.at(0).exportPath.isEmpty());

        QCOMPARE(entries.at(1).name, QStringLiteral("Talk"));
        QCOMPARE(entries.at(1).exportPath, QFileInfo(talkExport).absoluteFilePath());  // grouped

        QCOMPARE(entries.at(2).name, QStringLiteral("Screen only"));
        QVERIFY(!entries.at(2).isProject);
    }

    void exportThatWasDeletedIsNotOffered() {
        QTemporaryDir dir;
        makeProject(dir.filePath("Gone.screencam"), 1000, dir.filePath("Gone.mp4"), QDateTime::currentDateTime());
        const QList<RecordingEntry> entries = RecordingIndex::scan(dir.path());
        QCOMPARE(entries.size(), 1);
        QVERIFY(entries.at(0).exportPath.isEmpty());
    }

    void missingFolderIsEmpty() {
        QVERIFY(RecordingIndex::scan(QStringLiteral("/no/such/folder")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestRecordingIndex)
#include "tst_recording_index.moc"
