#include "app/QmlHotReloader.hpp"

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTextStream>

#include <cstdio>

QmlHotReloader::QmlHotReloader(QQmlApplicationEngine *engine, const QString &sourceDir, const QString &workDir,
                               QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_sourceDir(sourceDir)
    , m_qmlDir(QDir(sourceDir).filePath(QStringLiteral("qml")))
    , m_workDir(workDir) {
    QDir(m_workDir).removeRecursively();
    if (!QFileInfo(m_qmlDir).isDir() || !buildModule()) {
        return;
    }
    // Searched before qrc:/qt/qml, so the first load already comes from the source tree.
    m_engine->addImportPath(QFileInfo(m_moduleDir).path());
    m_enabled = true;

    // Editors (Neovim included) often save by writing a new file and renaming it over the old
    // one, after which the watcher has lost the file: re-add files on every event, and watch
    // the folders too. One save fires several events, so reload once things are quiet.
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &QmlHotReloader::reload);
    const auto changed = [this](const QString &) {
        watchFiles();
        m_debounce.start();
    };
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, changed);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, changed);
    watchFiles();
    std::fprintf(stderr, "[hot reload] watching %s\n", qPrintable(QDir::toNativeSeparators(m_qmlDir)));
}

bool QmlHotReloader::buildModule() {
    QFile compiled(QStringLiteral(":/qt/qml/ScreenCam/qmldir"));
    if (!compiled.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    // A new folder each time: Qt keeps a file's compiled form for as long as anything still
    // refers to it (a running animation, the tray icon, ...), so new URLs are the only sure
    // way to make it read the files again.
    const QDir module(QDir(m_workDir).filePath(QStringLiteral("%1/ScreenCam").arg(m_generation)));
    if (!QDir().mkpath(module.path())) {
        return false;
    }
    QFile qmldir(module.filePath(QStringLiteral("qmldir")));
    if (!qmldir.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    // A file only sees the module's types when the module's qmldir sits in the file's own
    // folder, so every file goes flat next to it. "prefer" is dropped: it would send the
    // engine back to the compiled copies.
    QTextStream in(&compiled);
    QTextStream out(&qmldir);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith(QLatin1String("prefer ")) || line.startsWith(QLatin1String("typeinfo "))) {
            continue;
        }
        if (line.endsWith(QLatin1String(".qml"))) {
            const qsizetype space = line.lastIndexOf(QLatin1Char(' '));
            const QString source = QDir(m_sourceDir).filePath(line.mid(space + 1));  // .../qml/components/X.qml
            const QString name = QFileInfo(source).fileName();
#if defined(Q_OS_WIN)
            const bool placed = QFile::copy(source, module.filePath(name));  // links need extra rights
#else
            const bool placed = QFile::link(source, module.filePath(name));
#endif
            if (!placed) {
                return false;
            }
            line = line.left(space + 1) + name;
        }
        out << line << '\n';
    }
    m_moduleDir = module.path();
    return true;
}

void QmlHotReloader::watchFiles() {
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    QStringList paths{m_qmlDir};
    QDirIterator it(m_qmlDir, {QStringLiteral("*.qml")}, QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        paths.append(it.next());
    }
    for (const QString &path : paths) {
        if (!watched.contains(path)) {
            m_watcher.addPath(path);
        }
    }
}

void QmlHotReloader::reload() {
    // Remember the main window's place and the open projects.
    QRect geometry;
    QStringList openProjects;
    for (QObject *root : m_engine->rootObjects()) {
        if (auto *window = qobject_cast<QQuickWindow *>(root)) {
            geometry = window->geometry();
            QVariant projects;
            QMetaObject::invokeMethod(window, "openProjects", Q_RETURN_ARG(QVariant, projects));
            openProjects = projects.toStringList();
        }
    }
    // Between the old window and the new one (or after a broken edit) there is no window at
    // all: that must not quit the app.
    QApplication::setQuitOnLastWindowClosed(false);
    const QList<QObject *> oldRoots = m_engine->rootObjects();
    for (QObject *root : oldRoots) {
        delete root;
    }
    m_engine->clearComponentCache();

    const QString previous = QFileInfo(m_moduleDir).path();
    m_generation++;
    if (!buildModule()) {
        std::fprintf(stderr, "[hot reload] failed - couldn't write %s\n", qPrintable(m_workDir));
        return;
    }
    QDir(previous).removeRecursively();
    m_engine->setInitialProperties({});  // don't reopen the project given on the command line
    m_engine->load(QUrl::fromLocalFile(QDir(m_moduleDir).filePath(QStringLiteral("Main.qml"))));

    const QList<QObject *> roots = m_engine->rootObjects();
    if (roots.isEmpty()) {
        std::fprintf(stderr, "[hot reload] failed - fix the error above and save again\n");
        return;
    }
    auto *window = qobject_cast<QQuickWindow *>(roots.constLast());
    if (window != nullptr && geometry.isValid()) {
        window->setGeometry(geometry);
    }
    for (const QString &project : openProjects) {
        QMetaObject::invokeMethod(window, "openEditor", Q_ARG(QString, project));
    }
    QApplication::setQuitOnLastWindowClosed(true);
    std::fprintf(stderr, "[hot reload] reloaded\n");
}
