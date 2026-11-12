// The FFmpeg-backend check: finds the plugin under any OS's file name, explains when it's missing.
#include "media/MediaBackendCheck.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

void touch(const QString &path) {
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
}

}  // namespace

class TestMediaBackend : public QObject {
    Q_OBJECT

private slots:
    void findsTheFfmpegPluginOnEveryPlatform_data() {
        QTest::addColumn<QString>("fileName");
        QTest::newRow("macOS") << "libffmpegmediaplugin.dylib";
        QTest::newRow("Linux") << "libffmpegmediaplugin.so";
        QTest::newRow("Windows") << "ffmpegmediaplugin.dll";
        QTest::newRow("Windows debug") << "ffmpegmediaplugind.dll";
    }

    void findsTheFfmpegPluginOnEveryPlatform() {
        QFETCH(QString, fileName);
        QTemporaryDir root;
        touch(root.filePath("multimedia/" + fileName));
        QVERIFY(MediaBackendCheck::hasFfmpegPlugin({root.path()}));
        QVERIFY(MediaBackendCheck::problem({root.path()}, QString()).isEmpty());
    }

    void reportsANativeOnlyQt() {
        // Exactly what a Qt built without FFmpeg looks like on macOS.
        QTemporaryDir root;
        touch(root.filePath("multimedia/libdarwinmediaplugin.dylib"));
        QVERIFY(!MediaBackendCheck::hasFfmpegPlugin({root.path()}));
        const QString message = MediaBackendCheck::problem({root.path()}, QString());
        QVERIFY(message.contains("FFmpeg"));
        QVERIFY(message.contains(root.path()));  // says where it looked
    }

    void reportsAForcedNativeBackend() {
        QTemporaryDir root;
        touch(root.filePath("multimedia/libffmpegmediaplugin.so"));
        QVERIFY(MediaBackendCheck::problem({root.path()}, "darwin").contains("QT_MEDIA_BACKEND=darwin"));
        QVERIFY(MediaBackendCheck::problem({root.path()}, "ffmpeg").isEmpty());
    }

    void thisBuildHasFfmpeg() {
        // The pipeline test needs it too; fail loudly with the reason if it's missing.
        const QString problem = MediaBackendCheck::currentProblem();
        QVERIFY2(problem.isEmpty(), qPrintable(problem));
    }
};

QTEST_MAIN(TestMediaBackend)
#include "tst_media_backend.moc"
