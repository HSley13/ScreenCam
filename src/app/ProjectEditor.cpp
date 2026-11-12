#include "app/ProjectEditor.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace {

constexpr int kAutosaveDelayMs = 600;  // long enough to coalesce a drag into one write

}  // namespace

ProjectEditor::ProjectEditor(QObject *parent)
    : QObject(parent)
    , m_style(new BubbleStyle(this, false)) {
    // Drags and slider moves change the style many times per second; saving each one would
    // hammer the disk, so a short single-shot timer batches them.
    m_autosave.setSingleShot(true);
    m_autosave.setInterval(kAutosaveDelayMs);
    connect(&m_autosave, &QTimer::timeout, this, [this] {
        save();
    });
    connect(m_style, &BubbleStyle::styleChanged, this, &ProjectEditor::onStyleEdited);
}

ProjectEditor::~ProjectEditor() {
    if (m_loaded && m_dirty) {
        save();
    }
}

bool ProjectEditor::open(const QString &location) {
    close();

    // Accept whatever QML hands us: a FolderDialog URL, a plain path, or the JSON file itself.
    QString path = location;
    const QUrl url(location);
    if (url.isLocalFile()) {
        path = url.toLocalFile();
    }
    QFileInfo info(path);
    if (info.isFile() && info.fileName() == QStringLiteral("project.json")) {
        path = info.absolutePath();
    }

    QString error;
    std::optional<Project> project = Project::load(QDir(path).absolutePath(), &error);
    if (!project.has_value()) {
        setErrorText(tr("Couldn't open “%1”: %2").arg(QFileInfo(path).fileName(), error));
        return false;
    }
    m_project = std::move(*project);
    m_recordedOffsetMs = m_project.cameraOffsetMs;
    m_loaded = true;
    m_current = 0;
    setErrorText(QString());
    loadCurrentStyle();
    emit projectChanged();
    emit cameraOffsetChanged();
    emit segmentsChanged();
    emit currentChanged();
    return true;
}

void ProjectEditor::close() {
    if (!m_loaded) {
        return;
    }
    if (m_dirty) {
        save();
    }
    m_autosave.stop();
    m_project = Project{};
    m_loaded = false;
    m_current = 0;
    emit projectChanged();
    emit segmentsChanged();
    emit currentChanged();
}

void ProjectEditor::selectAt(qint64 timeMs) {
    if (m_loaded) {
        select(m_project.segmentAt(timeMs));
    }
}

void ProjectEditor::select(int index) {
    if (!m_loaded) {
        return;
    }
    index = std::clamp(index, 0, segmentCount() - 1);
    if (index == m_current) {
        return;
    }
    m_current = index;
    loadCurrentStyle();
    emit currentChanged();
}

bool ProjectEditor::canSplitAt(qint64 timeMs) const {
    if (!m_loaded) {
        return false;
    }
    Project copy = m_project;  // splitAt() is the single source of the rule; try it on a copy
    return copy.splitAt(timeMs) >= 0;
}

int ProjectEditor::splitAt(qint64 timeMs) {
    if (!m_loaded) {
        return -1;
    }
    const int index = m_project.splitAt(timeMs);
    if (index < 0) {
        return -1;
    }
    m_current = index;  // the new part: that's what you want to move next
    loadCurrentStyle();
    emit segmentsChanged();
    emit currentChanged();
    markDirty();
    return index;
}

bool ProjectEditor::removeCurrent() {
    if (!m_loaded || !m_project.removeSegment(m_current)) {
        return false;
    }
    // Whoever inherited the time is now under the playhead: the previous segment, or the
    // (renumbered) first one when the first was removed.
    m_current = std::max(0, m_current - 1);
    loadCurrentStyle();
    emit segmentsChanged();
    emit currentChanged();
    markDirty();
    return true;
}

void ProjectEditor::applyToAll() {
    if (!m_loaded) {
        return;
    }
    const ProjectSegment &source = m_project.segments.at(static_cast<std::size_t>(m_current));
    for (ProjectSegment &segment : m_project.segments) {
        segment.style = source.style;
        segment.cameraVisible = source.cameraVisible;
    }
    emit segmentsChanged();
    markDirty();
}

qint64 ProjectEditor::segmentStart(int index) const {
    if (!m_loaded || index < 0 || index >= segmentCount()) {
        return 0;
    }
    return m_project.segments.at(static_cast<std::size_t>(index)).startMs;
}

qint64 ProjectEditor::segmentEnd(int index) const {
    if (!m_loaded || index < 0 || index >= segmentCount()) {
        return 0;
    }
    if (index + 1 < segmentCount()) {
        return m_project.segments.at(static_cast<std::size_t>(index + 1)).startMs;
    }
    return m_project.durationMs;
}

QString ProjectEditor::formatTime(qint64 ms) const {
    ms = std::max<qint64>(0, ms);
    const qint64 tenths = (ms / 100) % 10;
    const qint64 totalSeconds = ms / 1000;
    const qint64 seconds = totalSeconds % 60;
    const qint64 minutes = (totalSeconds / 60) % 60;
    const qint64 hours = totalSeconds / 3600;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3.%4")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'))
            .arg(tenths);
    }
    return QStringLiteral("%1:%2.%3").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0')).arg(tenths);
}

bool ProjectEditor::save() {
    m_autosave.stop();
    if (!m_loaded) {
        return false;
    }
    QString error;
    if (!m_project.save(&error)) {
        setErrorText(tr("Couldn't save the project: %1").arg(error));
        return false;
    }
    setErrorText(QString());
    if (m_dirty) {
        m_dirty = false;
        emit dirtyChanged();
    }
    return true;
}

void ProjectEditor::showInFileManager() const {
    if (m_loaded) {
        // Open the folder that *contains* the project, so it shows up selected-ish in context.
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_project.folder).absolutePath()));
    }
}

QString ProjectEditor::name() const {
    return m_loaded ? QFileInfo(m_project.folder).completeBaseName() : QString();
}

QUrl ProjectEditor::screenUrl() const {
    return m_loaded ? QUrl::fromLocalFile(m_project.screenPath()) : QUrl();
}

QUrl ProjectEditor::cameraUrl() const {
    return hasCamera() ? QUrl::fromLocalFile(m_project.cameraPath()) : QUrl();
}

void ProjectEditor::setDurationMs(qint64 durationMs) {
    if (!m_loaded || durationMs == m_project.durationMs) {
        return;
    }
    // Never shorter than the last cut: every segment must keep some time.
    if (durationMs < m_project.segments.back().startMs + Project::kMinSegmentMs) {
        return;
    }
    m_project.durationMs = durationMs;
    emit projectChanged();
    emit segmentsChanged();  // the last segment's end moved
    emit currentChanged();
    markDirty();
}

void ProjectEditor::setCameraOffsetMs(qint64 offsetMs) {
    if (!m_loaded || offsetMs == m_project.cameraOffsetMs) {
        return;
    }
    m_project.cameraOffsetMs = offsetMs;
    emit cameraOffsetChanged();
    markDirty();
}

QVariantList ProjectEditor::segments() const {
    QVariantList list;
    if (!m_loaded) {
        return list;
    }
    for (int i = 0; i < segmentCount(); i++) {
        const ProjectSegment &segment = m_project.segments.at(static_cast<std::size_t>(i));
        list.append(QVariantMap{
            {QStringLiteral("startMs"), segment.startMs},
            {QStringLiteral("endMs"), segmentEnd(i)},
            {QStringLiteral("cameraVisible"), segment.cameraVisible},
            {QStringLiteral("shape"), static_cast<int>(segment.style.shape)},
        });
    }
    return list;
}

bool ProjectEditor::cameraVisible() const {
    if (!m_loaded) {
        return false;
    }
    return m_project.segments.at(static_cast<std::size_t>(m_current)).cameraVisible;
}

void ProjectEditor::setCameraVisible(bool visible) {
    if (!m_loaded || visible == cameraVisible()) {
        return;
    }
    m_project.segments.at(static_cast<std::size_t>(m_current)).cameraVisible = visible;
    emit currentChanged();
    emit segmentsChanged();
    markDirty();
}

void ProjectEditor::loadCurrentStyle() {
    if (!m_loaded) {
        return;
    }
    // setSnapshot() emits styleChanged, which normally means "the user edited the bubble";
    // the flag tells onStyleEdited() that this one is us, not an edit.
    m_loadingStyle = true;
    m_style->setSnapshot(m_project.segments.at(static_cast<std::size_t>(m_current)).style);
    m_loadingStyle = false;
}

void ProjectEditor::onStyleEdited() {
    if (m_loadingStyle || !m_loaded) {
        return;
    }
    ProjectSegment &segment = m_project.segments.at(static_cast<std::size_t>(m_current));
    const bool shapeChanged = segment.style.shape != m_style->snapshot().shape;
    segment.style = m_style->snapshot();
    if (shapeChanged) {
        emit segmentsChanged();  // the timeline shows each segment's shape
    }
    markDirty();
}

void ProjectEditor::markDirty() {
    if (!m_dirty) {
        m_dirty = true;
        emit dirtyChanged();
    }
    m_autosave.start();  // restarts: saves once things have been quiet for a moment
}

void ProjectEditor::setErrorText(const QString &text) {
    if (text != m_errorText) {
        m_errorText = text;
        emit errorTextChanged();
    }
}
