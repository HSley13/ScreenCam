#include "app/ProjectEditor.hpp"

#ifdef SCREENCAM_HAS_EXPORT
#include "export/ProjectExporter.hpp"
#endif

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
    // A drag changes the style many times per second: save once it's quiet.
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
    emit durationChanged();
    emit cameraOffsetChanged();
    emit segmentsChanged();
    emit currentChanged();
    return true;
}

void ProjectEditor::close() {
    if (!m_loaded) {
        return;
    }
    cancelExport();
    if (m_dirty) {
        save();
    }
    m_autosave.stop();
    m_project = Project{};
    m_loaded = false;
    m_current = 0;
    emit projectChanged();
    emit durationChanged();
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
    Project copy = m_project;  // splitAt() owns the rule: try it on a copy
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
    m_current = index;
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
    // The segment that inherited the time.
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
    emit durationChanged();
    emit segmentsChanged();
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
    // Tells onStyleEdited() that this styleChanged is us, not an edit.
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
    m_autosave.start();
}

void ProjectEditor::setErrorText(const QString &text) {
    if (text != m_errorText) {
        m_errorText = text;
        emit errorTextChanged();
    }
}

bool ProjectEditor::exportAvailable() {
#ifdef SCREENCAM_HAS_EXPORT
    return true;
#else
    return false;
#endif
}

bool ProjectEditor::exporting() const {
#ifdef SCREENCAM_HAS_EXPORT
    return m_exporter != nullptr && m_exporter->isRunning();
#else
    return false;
#endif
}

QString ProjectEditor::exportPath() const {
    if (!m_loaded) {
        return QString();
    }
    if (!m_project.lastExport.isEmpty()) {
        return m_project.lastExport;  // replace it
    }
    // "Talk.screencam" -> "Talk.mp4", or "Talk 2.mp4" if taken.
    const QFileInfo projectInfo(m_project.folder);
    const QDir parent = projectInfo.absoluteDir();
    QString path = parent.filePath(projectInfo.completeBaseName() + QStringLiteral(".mp4"));
    for (int i = 2; QFileInfo::exists(path); i++) {
        path = parent.filePath(QStringLiteral("%1 %2.mp4").arg(projectInfo.completeBaseName()).arg(i));
    }
    return path;
}

bool ProjectEditor::exportVideo() {
#ifdef SCREENCAM_HAS_EXPORT
    if (!m_loaded || exporting()) {
        return false;
    }
    save();  // the export reads what's on disk
    if (m_exporter == nullptr) {
        m_exporter = new ProjectExporter(this);
        connect(m_exporter, &ProjectExporter::progress, this, [this](double ratio) {
            m_exportProgress = ratio;
            emit exportProgressChanged();
        });
        connect(m_exporter, &ProjectExporter::finished, this, [this](const QString &path) {
            m_project.lastExport = path;
            save();
            emit exportStateChanged();
            emit exportFinished(path);
        });
        connect(m_exporter, &ProjectExporter::failed, this, [this](const QString &message) {
            setErrorText(tr("Export failed: %1").arg(message));
            emit exportStateChanged();
        });
        connect(m_exporter, &ProjectExporter::canceled, this, &ProjectEditor::exportStateChanged);
    }
    m_exportProgress = 0.0;
    emit exportProgressChanged();
    setErrorText(QString());
    const bool started = m_exporter->start(m_project, exportPath());
    emit exportStateChanged();
    return started;
#else
    setErrorText(tr("This build of ScreenCam can't export (it was built without FFmpeg)."));
    return false;
#endif
}

void ProjectEditor::cancelExport() {
#ifdef SCREENCAM_HAS_EXPORT
    if (m_exporter != nullptr) {
        m_exporter->cancel();
    }
#endif
}

void ProjectEditor::playLastExport() const {
    if (!m_project.lastExport.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_project.lastExport));
    }
}
