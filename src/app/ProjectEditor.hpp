#pragma once

#include "app/BubbleStyle.hpp"
#include "media/Project.hpp"

#include <QObject>
#include <QSize>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

/// The editing model behind the editor page (QML type `ProjectEditor`, one per open project).
///
/// Why a C++ object and not plain QML: the timeline rules (where a cut may go, who inherits a
/// deleted segment's time) live in core/Timeline + media/Project and are unit-tested there; the
/// exporter reads the very same project.json. QML only displays this and forwards gestures.
///
/// The "current" segment is the one under the playhead. Its look is exposed as `style`, a
/// regular BubbleStyle, so the existing bubble UI (CameraBubble drag/resize, BubbleSection
/// sliders) edits a segment exactly like it edits the recording defaults. Every change is
/// copied back into the segment and saved automatically shortly after.
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
    Q_PROPERTY(qint64 durationMs READ durationMs WRITE setDurationMs NOTIFY projectChanged)
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

public:
    explicit ProjectEditor(QObject *parent = nullptr);
    ~ProjectEditor() override;  // saves pending changes

    /// Opens a ".screencam" folder (a path, a file:// URL, or the project.json inside it).
    Q_INVOKABLE bool open(const QString &location);
    /// Saves pending changes and forgets the project.
    Q_INVOKABLE void close();

    /// Makes the segment playing at `timeMs` current (called as the playhead moves).
    Q_INVOKABLE void selectAt(qint64 timeMs);
    void select(int index);

    Q_INVOKABLE bool canSplitAt(qint64 timeMs) const;
    /// Cuts the segment under `timeMs`; the part after the cut becomes current.
    Q_INVOKABLE int splitAt(qint64 timeMs);
    /// Deletes the current segment (its neighbour takes over its time).
    Q_INVOKABLE bool removeCurrent();
    /// Copies the current segment's look and position to every segment.
    Q_INVOKABLE void applyToAll();

    Q_INVOKABLE qint64 segmentStart(int index) const;
    Q_INVOKABLE qint64 segmentEnd(int index) const;
    /// "1:05.3" — the editor needs tenths of a second, the recorder's timer doesn't.
    Q_INVOKABLE QString formatTime(qint64 ms) const;

    /// Writes project.json now (normally done automatically after each change).
    Q_INVOKABLE bool save();
    Q_INVOKABLE void showInFileManager() const;

    bool loaded() const { return m_loaded; }
    QString folder() const { return m_project.folder; }
    QString name() const;
    QUrl screenUrl() const;
    QUrl cameraUrl() const;
    bool hasCamera() const { return m_loaded && !m_project.cameraFile.isEmpty(); }
    QSize canvasSize() const { return m_project.canvasSize; }
    qint64 durationMs() const { return m_project.durationMs; }
    /// The player knows the real length; the recorder's estimate may be off by a few frames.
    void setDurationMs(qint64 durationMs);
    qint64 cameraOffsetMs() const { return m_project.cameraOffsetMs; }
    /// Fine-tune lip sync between the two files.
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

signals:
    void projectChanged();
    void segmentsChanged();
    void currentChanged();
    void cameraOffsetChanged();
    void dirtyChanged();
    void errorTextChanged();

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
    bool m_loadingStyle = false;  // true while we push a segment into m_style ourselves
    bool m_dirty = false;
    QString m_errorText;
    QTimer m_autosave;
};
