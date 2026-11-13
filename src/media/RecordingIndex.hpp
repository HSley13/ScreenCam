#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

/// One recording in the output folder, as the "Recordings" list shows it.
///
/// Two kinds live side by side there:
///   - "Name.screencam" — a project folder (screen.mp4 + camera.mp4 + project.json). That's
///     the editable recording: it's opened in the editor, never played directly.
///   - "Name.mp4" — a finished video: a Screen-only / Camera-only recording, or the export of
///     a project. An export is listed *with its project* (exportPath), not on its own.
struct RecordingEntry {
    QString path;        // the .screencam folder or the .mp4 file
    QString name;        // file name without extension
    bool isProject = false;
    QDateTime modified;  // when it was last recorded / edited
    qint64 durationMs = 0;  // projects only (videos would need decoding)
    QString exportPath;  // projects: the exported video, if it still exists
};

namespace RecordingIndex {

/// Everything in `folder` that is a project or a video, newest first. Folders that only look
/// like projects (no readable project.json) are skipped.
QList<RecordingEntry> scan(const QString &folder);

}  // namespace RecordingIndex
