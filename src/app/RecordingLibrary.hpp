#pragma once

#include "media/RecordingIndex.hpp"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/// The "Recordings" list (QML singleton `Library`), re-read when the output folder or its
/// contents change and whenever the list is opened.
class RecordingLibrary : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Library)
    QML_SINGLETON

    // [{ path, name, isProject, date, duration, exportPath }]
    Q_PROPERTY(QVariantList recordings READ recordings NOTIFY recordingsChanged)

public:
    static RecordingLibrary *instance();
    static RecordingLibrary *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QVariantList recordings() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void play(const QString &path) const;
    /// Selects it in Finder / Explorer (opens the folder elsewhere).
    Q_INVOKABLE void reveal(const QString &path) const;

signals:
    void recordingsChanged();

private:
    explicit RecordingLibrary(QObject *parent = nullptr);
    void watchOutputFolder();

    QList<RecordingEntry> m_entries;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;  // a recording being written changes the folder many times
};
