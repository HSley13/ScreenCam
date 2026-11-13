#pragma once

#include <QString>
#include <QStringList>

/// Whether Qt Multimedia's FFmpeg backend is available. ScreenCam needs it (screen capture,
/// QVideoFrameInput); a Qt without it silently falls back to a native backend where the
/// preview just stays empty, so the app checks up front and explains.
namespace MediaBackendCheck {

/// Qt's plugin dir plus QCoreApplication::libraryPaths().
QStringList pluginRoots();

bool hasFfmpegPlugin(const QStringList &roots);

/// Empty if the FFmpeg backend can be used, else a message for the user.
/// `forcedBackend`: QT_MEDIA_BACKEND.
QString problem(const QStringList &roots, const QString &forcedBackend);

QString currentProblem();

}  // namespace MediaBackendCheck
