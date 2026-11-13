#include "media/Project.hpp"

#include "core/Timeline.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace core = screencam::core;

namespace {

std::vector<std::int64_t> startTimes(const std::vector<ProjectSegment> &segments) {
    std::vector<std::int64_t> starts;
    starts.reserve(segments.size());
    for (const ProjectSegment &segment : segments) {
        starts.push_back(segment.startMs);
    }
    return starts;
}

QJsonObject styleToJson(const BubbleStyleSnapshot &style) {
    return QJsonObject{
        {"shape", static_cast<int>(style.shape)},
        {"size", style.size},
        {"roundness", style.roundness},
        {"mirrored", style.mirrored},
        {"borderWidth", style.borderWidth},
        {"borderColor", style.borderColor.name(QColor::HexArgb)},
        {"shadow", style.shadow},
        {"centerX", style.center.x},
        {"centerY", style.center.y},
    };
}

BubbleStyleSnapshot styleFromJson(const QJsonObject &json) {
    BubbleStyleSnapshot style;  // defaults fill anything missing
    style.shape = static_cast<core::BubbleShape>(json.value("shape").toInt(static_cast<int>(style.shape)));
    style.size = json.value("size").toDouble(style.size);
    style.roundness = json.value("roundness").toDouble(style.roundness);
    style.mirrored = json.value("mirrored").toBool(style.mirrored);
    style.borderWidth = json.value("borderWidth").toDouble(style.borderWidth);
    style.borderColor = QColor(json.value("borderColor").toString(style.borderColor.name(QColor::HexArgb)));
    style.shadow = json.value("shadow").toBool(style.shadow);
    style.center.x = json.value("centerX").toDouble(style.center.x);
    style.center.y = json.value("centerY").toDouble(style.center.y);
    return style;
}

}  // namespace

QString Project::screenPath() const {
    return QDir(folder).filePath(screenFile);
}

QString Project::cameraPath() const {
    return cameraFile.isEmpty() ? QString() : QDir(folder).filePath(cameraFile);
}

QString Project::jsonPath() const {
    return QDir(folder).filePath(QStringLiteral("project.json"));
}

int Project::segmentAt(qint64 timeMs) const {
    return core::segmentAt(startTimes(segments), timeMs);
}

int Project::splitAt(qint64 timeMs) {
    const std::vector<std::int64_t> starts = startTimes(segments);
    if (!core::canSplitAt(starts, timeMs, durationMs, kMinSegmentMs)) {
        return -1;
    }
    const std::size_t index = core::insertionIndex(starts, timeMs);
    ProjectSegment segment = segments.at(index - 1);  // continue with the current layout
    segment.startMs = timeMs;
    segments.insert(segments.begin() + static_cast<std::ptrdiff_t>(index), segment);
    return static_cast<int>(index);
}

bool Project::removeSegment(int index) {
    if (segments.size() <= 1 || index < 0 || index >= static_cast<int>(segments.size())) {
        return false;  // the last remaining segment can't go
    }
    if (index == 0) {
        segments.at(1).startMs = 0;  // the next segment now starts at the beginning
    }
    segments.erase(segments.begin() + index);
    return true;
}

bool Project::save(QString *error) const {
    QJsonArray segmentArray;
    for (const ProjectSegment &segment : segments) {
        segmentArray.append(QJsonObject{
            {"startMs", segment.startMs},
            {"cameraVisible", segment.cameraVisible},
            {"style", styleToJson(segment.style)},
        });
    }
    const QJsonObject root{
        {"format", "ScreenCam project"},
        {"version", kFormatVersion},
        {"screenFile", screenFile},
        {"cameraFile", cameraFile},
        {"durationMs", durationMs},
        {"cameraOffsetMs", cameraOffsetMs},
        {"width", canvasSize.width()},
        {"height", canvasSize.height()},
        {"framesPerSecond", framesPerSecond},
        {"createdAt", createdAt},
        {"lastExport", lastExport},
        {"segments", segmentArray},
    };
    // Written to a temporary file and renamed: a crash never leaves half a file.
    QSaveFile file(jsonPath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

std::optional<Project> Project::load(const QString &folder, QString *error) {
    auto fail = [error](const QString &message) -> std::optional<Project> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };

    Project project;
    project.folder = QDir(folder).absolutePath();
    QFile file(project.jsonPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("Can't open %1: %2").arg(project.jsonPath(), file.errorString()));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        return fail(QStringLiteral("%1 is not a valid project file: %2").arg(project.jsonPath(), parseError.errorString()));
    }
    const QJsonObject root = document.object();
    if (root.value("version").toInt() > kFormatVersion) {
        return fail(QStringLiteral("This project was made by a newer ScreenCam."));
    }

    project.screenFile = root.value("screenFile").toString(project.screenFile);
    project.cameraFile = root.value("cameraFile").toString();
    project.durationMs = root.value("durationMs").toInteger();
    project.cameraOffsetMs = root.value("cameraOffsetMs").toInteger();
    project.canvasSize = QSize(root.value("width").toInt(), root.value("height").toInt());
    project.framesPerSecond = root.value("framesPerSecond").toInt(30);
    project.createdAt = root.value("createdAt").toString();
    project.lastExport = root.value("lastExport").toString();
    for (const QJsonValue &value : root.value("segments").toArray()) {
        const QJsonObject json = value.toObject();
        ProjectSegment segment;
        segment.startMs = json.value("startMs").toInteger();
        segment.cameraVisible = json.value("cameraVisible").toBool(true);
        segment.style = styleFromJson(json.value("style").toObject());
        project.segments.push_back(segment);
    }

    // Repair anything a hand-edited file might break: sorted, first segment at 0.
    std::sort(project.segments.begin(), project.segments.end(),
              [](const ProjectSegment &a, const ProjectSegment &b) { return a.startMs < b.startMs; });
    if (project.segments.empty()) {
        project.segments.push_back(ProjectSegment{});
    }
    project.segments.front().startMs = 0;
    return project;
}
