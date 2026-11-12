#pragma once

#include <QString>
#include <QStringList>

/// Detects whether Qt Multimedia's FFmpeg backend is available.
///
/// Why this matters: ScreenCam needs FFmpeg-only features — QScreenCapture (screen preview and
/// recording) and QVideoFrameInput (feeding composited frames to the encoder). A Qt built
/// without FFmpeg (e.g. compiled from source without -ffmpeg-dir, or Homebrew's Qt) silently
/// falls back to the native backend ("darwin" on macOS), where the screen preview simply stays
/// empty. Checking up front lets the app explain the problem instead.
namespace MediaBackendCheck {

/// Directories Qt loads plugins from: its install plugin dir + QCoreApplication::libraryPaths().
QStringList pluginRoots();

/// True if any root contains multimedia/<...>ffmpegmediaplugin<...> (any OS's file naming).
bool hasFfmpegPlugin(const QStringList &roots);

/// Empty if the FFmpeg backend can be used; otherwise a message for the user.
/// `forcedBackend` is the value of QT_MEDIA_BACKEND (empty when unset).
QString problem(const QStringList &roots, const QString &forcedBackend);

/// problem() for this process.
QString currentProblem();

}  // namespace MediaBackendCheck
