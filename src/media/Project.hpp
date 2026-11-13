#pragma once

#include "media/BubbleStyleSnapshot.hpp"

#include <QSize>
#include <QString>

#include <optional>
#include <vector>

/// Lasts from `startMs` until the next segment starts.
struct ProjectSegment {
    qint64 startMs = 0;
    BubbleStyleSnapshot style;
    bool cameraVisible = true;
};

/// An editable recording, stored as a "<name>.screencam" folder: screen.mp4 (with the audio),
/// camera.mp4 (full frame, unmirrored) and project.json (this structure).
struct Project {
    static constexpr int kFormatVersion = 1;
    static constexpr qint64 kMinSegmentMs = 500;

    QString folder;  // not stored in the JSON
    QString screenFile = QStringLiteral("screen.mp4");
    QString cameraFile = QStringLiteral("camera.mp4");
    qint64 durationMs = 0;
    // camera time = video time - cameraOffsetMs
    qint64 cameraOffsetMs = 0;
    QSize canvasSize;
    int framesPerSecond = 30;
    QString createdAt;  // ISO 8601
    QString lastExport;
    std::vector<ProjectSegment> segments;  // sorted by startMs; segments[0].startMs == 0

    QString screenPath() const;
    QString cameraPath() const;
    QString jsonPath() const;

    int segmentAt(qint64 timeMs) const;
    /// The new segment's index, or -1 if the cut is too close to another one.
    int splitAt(qint64 timeMs);
    /// Removes a segment; the previous one (or the next, for the first) takes over its time.
    bool removeSegment(int index);

    bool save(QString *error = nullptr) const;
    static std::optional<Project> load(const QString &folder, QString *error = nullptr);
};
