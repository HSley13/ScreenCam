#include "media/RecordingIndex.hpp"

#include "media/Project.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace RecordingIndex {

QList<RecordingEntry> scan(const QString &folder) {
    QList<RecordingEntry> entries;
    QSet<QString> exports;  // videos that belong to a project: not listed separately

    const QDir dir(folder);
    // 1) Projects.
    for (const QFileInfo &info : dir.entryInfoList({QStringLiteral("*.screencam")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const std::optional<Project> project = Project::load(info.absoluteFilePath());
        if (!project.has_value()) {
            continue;
        }
        RecordingEntry entry;
        entry.path = info.absoluteFilePath();
        entry.name = info.completeBaseName();
        entry.isProject = true;
        // project.json changes on every edit; the folder's own date doesn't.
        entry.modified = QFileInfo(project->jsonPath()).lastModified();
        entry.durationMs = project->durationMs;
        if (!project->lastExport.isEmpty() && QFileInfo::exists(project->lastExport)) {
            entry.exportPath = QFileInfo(project->lastExport).absoluteFilePath();
            exports.insert(entry.exportPath);
        }
        entries.append(entry);
    }
    // 2) Finished videos.
    for (const QFileInfo &info : dir.entryInfoList({QStringLiteral("*.mp4")}, QDir::Files)) {
        if (exports.contains(info.absoluteFilePath())) {
            continue;
        }
        RecordingEntry entry;
        entry.path = info.absoluteFilePath();
        entry.name = info.completeBaseName();
        entry.modified = info.lastModified();
        entries.append(entry);
    }
    std::sort(entries.begin(), entries.end(), [](const RecordingEntry &a, const RecordingEntry &b) {
        return a.modified > b.modified;
    });
    return entries;
}

}  // namespace RecordingIndex
