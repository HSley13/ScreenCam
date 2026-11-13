#pragma once

#include "app/BubbleStyle.hpp"
#include "media/Project.hpp"

#include <QObject>
#include <QSize>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class ProjectExporter;

/// The editor's model (QML type `ProjectEditor`, one per open project); the timeline rules
/// live in core/Timeline and media/Project. The segment under the playhead is exposed as
/// `style`, a regular BubbleStyle, so the same bubble UI edits it as edits the recording
/// defaults. Changes are saved automatically shortly after.
class ProjectEditor : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool loaded READ loaded NOTIFY projectChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY projectChanged)
    Q_PROPERTY(QString name READ name NOTIFY projectChanged)
    Q_PROPERTY(QUrl screenUrl READ screenUrl NOTIFY projectChanged)
    Q_PROPERTY(QUrl cameraUrl READ cameraUrl NOTIFY projectChanged)
    Q_PROPERTY(bool hasCamera READ hasCamera NOTIFY projectChanged)
    Q_PROPERTY(QSize canvasSize READ canvasSize NOTIFY projectChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs WRITE setDurationMs NOTIFY durationChanged)
    Q_PROPERTY(qint64 cameraOffsetMs READ cameraOffsetMs WRITE setCameraOffsetMs NOTIFY cameraOffsetChanged)
    Q_PROPERTY(qint64 recordedCameraOffsetMs READ recordedCameraOffsetMs NOTIFY projectChanged)
    Q_PROPERTY(QVariantList segments READ segments NOTIFY segmentsChanged)
    Q_PROPERTY(int segmentCount READ segmentCount NOTIFY segmentsChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE select NOTIFY currentChanged)
    Q_PROPERTY(qint64 currentStartMs READ currentStartMs NOTIFY currentChanged)
    Q_PROPERTY(qint64 currentEndMs READ currentEndMs NOTIFY currentChanged)
    Q_PROPERTY(bool cameraVisible READ cameraVisible WRITE setCameraVisible NOTIFY currentChanged)
    Q_PROPERTY(BubbleStyle *style READ style CONSTANT)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    // False when built without the FFmpeg development files.
    Q_PROPERTY(bool exportAvailable READ exportAvailable CONSTANT)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportStateChanged)
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportProgressChanged)
    Q_PROPERTY(QString lastExport READ lastExport NOTIFY exportStateChanged)

public:
    explicit ProjectEditor(QObject *parent = nullptr);
    ~ProjectEditor() override;  // saves pending changes

    /// A path, a file:// URL, or the project.json inside the folder.
    Q_INVOKABLE bool open(const QString &location);
    Q_INVOKABLE void close();

    Q_INVOKABLE void selectAt(qint64 timeMs);
    void select(int index);

    Q_INVOKABLE bool canSplitAt(qint64 timeMs) const;
    /// The part after the cut becomes current.
    Q_INVOKABLE int splitAt(qint64 timeMs);
    Q_INVOKABLE bool removeCurrent();
    Q_INVOKABLE void applyToAll();

    Q_INVOKABLE qint64 segmentStart(int index) const;
    Q_INVOKABLE qint64 segmentEnd(int index) const;
    /// "1:05.3"
    Q_INVOKABLE QString formatTime(qint64 ms) const;

    Q_INVOKABLE bool save();
    Q_INVOKABLE void showInFileManager() const;

    /// "<name>.mp4" next to the project, in the background; replaces the previous export.
    Q_INVOKABLE bool exportVideo();
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void playLastExport() const;

    bool loaded() const { return m_loaded; }
    QString folder() const { return m_project.folder; }
    QString name() const;
    QUrl screenUrl() const;
    QUrl cameraUrl() const;
    bool hasCamera() const { return m_loaded && !m_project.cameraFile.isEmpty(); }
    QSize canvasSize() const { return m_project.canvasSize; }
    qint64 durationMs() const { return m_project.durationMs; }
    /// The player's length; the recorder's estimate may be off by a few frames.
    void setDurationMs(qint64 durationMs);
    qint64 cameraOffsetMs() const { return m_project.cameraOffsetMs; }
    void setCameraOffsetMs(qint64 offsetMs);
    qint64 recordedCameraOffsetMs() const { return m_recordedOffsetMs; }
    QVariantList segments() const;
    int segmentCount() const { return static_cast<int>(m_project.segments.size()); }
    int currentIndex() const { return m_current; }
    qint64 currentStartMs() const { return segmentStart(m_current); }
    qint64 currentEndMs() const { return segmentEnd(m_current); }
    bool cameraVisible() const;
    void setCameraVisible(bool visible);
    BubbleStyle *style() const { return m_style; }
    bool dirty() const { return m_dirty; }
    QString errorText() const { return m_errorText; }
    static bool exportAvailable();
    bool exporting() const;
    double exportProgress() const { return m_exportProgress; }
    QString lastExport() const { return m_project.lastExport; }
    QString exportPath() const;

signals:
    void projectChanged();
    void durationChanged();  // separate, so QML doesn't reload the videos
    void segmentsChanged();
    void currentChanged();
    void cameraOffsetChanged();
    void dirtyChanged();
    void errorTextChanged();
    void exportStateChanged();
    void exportProgressChanged();
    void exportFinished(const QString &path);

private:
    void loadCurrentStyle();  // segment -> m_style
    void onStyleEdited();     // m_style -> segment
    void markDirty();
    void setErrorText(const QString &text);

    Project m_project;
    bool m_loaded = false;
    int m_current = 0;
    qint64 m_recordedOffsetMs = 0;
    BubbleStyle *m_style = nullptr;
    bool m_loadingStyle = false;  // pushing a segment into m_style ourselves
    bool m_dirty = false;
    QString m_errorText;
    QTimer m_autosave;
    ProjectExporter *m_exporter = nullptr;
    double m_exportProgress = 0.0;
};
