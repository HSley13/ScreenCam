#include "media/MediaBackendCheck.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>

namespace MediaBackendCheck {

QStringList pluginRoots() {
    QStringList roots{QLibraryInfo::path(QLibraryInfo::PluginsPath)};
    for (const QString &path : QCoreApplication::libraryPaths()) {
        if (!roots.contains(path)) {
            roots.append(path);
        }
    }
    return roots;
}

bool hasFfmpegPlugin(const QStringList &roots) {
    for (const QString &root : roots) {
        // libffmpegmediaplugin.dylib / .so, ffmpegmediaplugin.dll, ffmpegmediaplugind.dll (debug)
        const QDir dir(QDir(root).filePath(QStringLiteral("multimedia")));
        if (!dir.entryList({QStringLiteral("*ffmpegmediaplugin*")}, QDir::Files).isEmpty()) {
            return true;
        }
    }
    return false;
}

QString problem(const QStringList &roots, const QString &forcedBackend) {
    if (!forcedBackend.isEmpty() && forcedBackend.compare(QStringLiteral("ffmpeg"), Qt::CaseInsensitive) != 0) {
        return QCoreApplication::translate("MediaBackendCheck",
                                           "QT_MEDIA_BACKEND=%1 disables Qt's FFmpeg backend, which ScreenCam needs for "
                                           "screen capture and recording. Unset it and restart.")
            .arg(forcedBackend);
    }
    if (hasFfmpegPlugin(roots)) {
        return {};
    }
    return QCoreApplication::translate(
               "MediaBackendCheck",
               "This Qt was built without the FFmpeg multimedia backend, so the screen can't be captured or "
               "recorded. Install Qt with the Qt Online Installer (it includes FFmpeg) or rebuild Qt with "
               "-ffmpeg-dir — see README ▸ Troubleshooting. (Looked for multimedia/*ffmpegmediaplugin* in: %1)")
        .arg(roots.join(QStringLiteral(", ")));
}

QString currentProblem() {
    return problem(pluginRoots(), qEnvironmentVariable("QT_MEDIA_BACKEND"));
}

}  // namespace MediaBackendCheck
