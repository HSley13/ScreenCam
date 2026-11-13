#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

/// A "Recordings" list entry: a "Name.screencam" project, or a "Name.mp4" video. A project's
/// export is listed with its project (exportPath), not on its own.
struct RecordingEntry {
    QString path;
    QString name;
    bool isProject = false;
    QDateTime modified;
    qint64 durationMs = 0;  // projects only (videos would need decoding)
    QString exportPath;
};

namespace RecordingIndex {

/// Newest first; folders without a readable project.json are skipped.
QList<RecordingEntry> scan(const QString &folder);

}  // namespace RecordingIndex
