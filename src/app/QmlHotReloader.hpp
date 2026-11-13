#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

class QQmlApplicationEngine;

/// Debug builds: save a .qml file and the UI reloads in place, without a rebuild.
///
/// The QML stays compiled into the ScreenCam module, so the engine is also given a copy of
/// the module on disk, found before the compiled one: the same qmldir, with links to the
/// source files (copies on Windows). C++ state survives a reload; the main window keeps its
/// place and open editors are reopened.
class QmlHotReloader : public QObject {
    Q_OBJECT

public:
    /// `sourceDir` holds qml/, `workDir` gets the module copy. Call before the first load.
    QmlHotReloader(QQmlApplicationEngine *engine, const QString &sourceDir, const QString &workDir,
                   QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }

private:
    bool buildModule();
    void watchFiles();
    void reload();

    QQmlApplicationEngine *m_engine;
    QString m_sourceDir;
    QString m_qmlDir;
    QString m_workDir;
    QString m_moduleDir;  // <work>/<generation>/ScreenCam
    int m_generation = 0;
    bool m_enabled = false;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
