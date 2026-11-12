#pragma once

#include "media/BubbleStyleSnapshot.hpp"

#include <QSize>
#include <QString>

#include <optional>
#include <vector>

/// One stretch of the video with its own camera bubble layout. It lasts from `startMs` until the
/// next segment starts (or the video ends).
struct ProjectSegment {
    qint64 startMs = 0;
    BubbleStyleSnapshot style;
    bool cameraVisible = true;  // false: no bubble during this segment
};

/// A recording that can still be edited: separate screen and camera videos + the timeline that
/// says where the camera bubble goes. Stored as a "<name>.screencam" folder:
///   screen.mp4   — screen / window / region, with the microphone audio
///   camera.mp4   — the camera's full frame, unmirrored, no audio
///   project.json — this structure
struct Project {
    static constexpr int kFormatVersion = 1;
    static constexpr qint64 kMinSegmentMs = 500;

    QString folder;  // absolute path of the .screencam folder (not stored in the JSON)
    QString screenFile = QStringLiteral("screen.mp4");
    QString cameraFile = QStringLiteral("camera.mp4");  // empty: recorded without camera
    qint64 durationMs = 0;
    // The camera's clock started this much later than the screen's:
    // camera time = video time - cameraOffsetMs.
    qint64 cameraOffsetMs = 0;
    QSize canvasSize;  // size of the exported video
    int framesPerSecond = 30;
    QString createdAt;  // ISO 8601
    QString lastExport;  // path of the latest exported video, if any
    std::vector<ProjectSegment> segments;  // sorted by startMs; segments[0].startMs == 0

    QString screenPath() const;
    QString cameraPath() const;
    QString jsonPath() const;

    /// Index of the segment playing at `timeMs`.
    int segmentAt(qint64 timeMs) const;
    /// Cuts the segment playing at `timeMs` in two; the new one starts with the same layout.
    /// Returns the new segment's index, or -1 if the cut is too close to another one.
    int splitAt(qint64 timeMs);
    /// Removes a segment; the previous one (or the next, for the first) takes over its time.
    bool removeSegment(int index);

    bool save(QString *error = nullptr) const;
    static std::optional<Project> load(const QString &folder, QString *error = nullptr);
};
