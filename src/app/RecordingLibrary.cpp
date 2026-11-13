#include "app/RecordingLibrary.hpp"

#include "app/RecordingSettings.hpp"
#include "core/TimeFormat.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QLocale>
#include <QProcess>
#include <QUrl>

RecordingLibrary *RecordingLibrary::instance() {
    static RecordingLibrary *library = new RecordingLibrary(qApp);
    return library;
}

RecordingLibrary *RecordingLibrary::create(QQmlEngine *, QJSEngine *) {
    RecordingLibrary *library = instance();
    QJSEngine::setObjectOwnership(library, QJSEngine::CppOwnership);
    return library;
}

RecordingLibrary::RecordingLibrary(QObject *parent)
    : QObject(parent) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(500);
    connect(&m_debounce, &QTimer::timeout, this, &RecordingLibrary::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(RecordingSettings::instance(), &RecordingSettings::outputFolderChanged, this, [this] {
        watchOutputFolder();
        refresh();
    });
    watchOutputFolder();
    refresh();
}

void RecordingLibrary::watchOutputFolder() {
    if (!m_watcher.directories().isEmpty()) {
        m_watcher.removePaths(m_watcher.directories());
    }
    const QString folder = RecordingSettings::instance()->outputFolder();
    if (QFileInfo(folder).isDir()) {
        m_watcher.addPath(folder);
    }
}

void RecordingLibrary::refresh() {
    m_entries = RecordingIndex::scan(RecordingSettings::instance()->outputFolder());
    if (m_watcher.directories().isEmpty()) {
        watchOutputFolder();  // the folder may only exist since the first recording
    }
    emit recordingsChanged();
}

QVariantList RecordingLibrary::recordings() const {
    const QLocale locale;
    const QDate today = QDate::currentDate();
    QVariantList list;
    for (const RecordingEntry &entry : m_entries) {
        // "Today 11:32", "Yesterday 18:42", "12 Nov 2026 18:42".
        QString date;
        const QString time = locale.toString(entry.modified.time(), QLocale::ShortFormat);
        if (entry.modified.date() == today) {
            date = tr("Today %1").arg(time);
        } else if (entry.modified.date() == today.addDays(-1)) {
            date = tr("Yesterday %1").arg(time);
        } else {
            date = locale.toString(entry.modified, QLocale::ShortFormat);
        }
        list.append(QVariantMap{
            {QStringLiteral("path"), entry.path},
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("isProject"), entry.isProject},
            {QStringLiteral("date"), date},
            {QStringLiteral("duration"),
             entry.durationMs > 0
                 ? QString::fromStdString(screencam::core::formatElapsed(static_cast<double>(entry.durationMs) / 1000.0))
                 : QString()},
            {QStringLiteral("exportPath"), entry.exportPath},
        });
    }
    return list;
}

void RecordingLibrary::play(const QString &path) const {
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void RecordingLibrary::reveal(const QString &path) const {
#if defined(Q_OS_MACOS)
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), path});
#elif defined(Q_OS_WIN)
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            {QStringLiteral("/select,") + QDir::toNativeSeparators(path)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}
