#include "app/QmlHotReloader.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileOpenEvent>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

namespace {

/// A project double-clicked in Finder arrives as a QFileOpenEvent, also at launch: forward it
/// to Main.qml's openEditor(), or keep it until the UI exists.
class FileOpenHandler : public QObject {
public:
    explicit FileOpenHandler(QQmlApplicationEngine *engine)
        : m_engine(engine) {}

    QString takePending() {
        QString path = m_pending;
        m_pending.clear();
        return path;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() != QEvent::FileOpen) {
            return QObject::eventFilter(watched, event);
        }
        const QString path = static_cast<QFileOpenEvent *>(event)->file();
        if (m_engine->rootObjects().isEmpty()) {
            m_pending = path;
        } else {
            QMetaObject::invokeMethod(m_engine->rootObjects().constFirst(), "openEditor", Q_ARG(QString, path));
        }
        return true;
    }

private:
    QQmlApplicationEngine *m_engine;
    QString m_pending;
};

} // namespace

int main(int argc, char *argv[]) {
    // Not QGuiApplication: Qt.labs.platform's tray icon uses Qt Widgets on Windows and Linux.
    QApplication app(argc, argv);
    QApplication::setOrganizationName("hsley13");
    QApplication::setOrganizationDomain("hsley13.github.io");
    QApplication::setApplicationName("ScreenCam");
    QApplication::setApplicationDisplayName("ScreenCam");
    QApplication::setApplicationVersion(SCREENCAM_VERSION);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/screencam/screencam.png")));

    // Native styles ignore most custom backgrounds and colours.
    QQuickStyle::setStyle("Basic");

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Screen recorder with a draggable camera bubble"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption selfTest(
        QStringLiteral("self-test"),
        QStringLiteral(
            "Load the UI, run for two seconds and exit; non-zero if QML reported any warning (used by CTest/CI)."));
    parser.addOption(selfTest);
    const QCommandLineOption screenshot(
        QStringLiteral("screenshot"),
        QStringLiteral("With --self-test: save a picture of the window to <file> before exiting."),
        QStringLiteral("file"));
    parser.addOption(screenshot);
    const QCommandLineOption openProject(
        QStringLiteral("open"), QStringLiteral("Open a .screencam project in the editor."), QStringLiteral("project"));
    parser.addOption(openProject);
    // What the OS passes when a project is dropped on the app.
    parser.addPositionalArgument(QStringLiteral("project"), QStringLiteral("Optional .screencam project to open."));
    parser.process(app);

    QString initialProject = parser.value(openProject);
    if (initialProject.isEmpty() && !parser.positionalArguments().isEmpty()) {
        initialProject = parser.positionalArguments().constFirst();
    }

    QQmlApplicationEngine engine;

    // --self-test fails on any QML warning.
    int qmlWarnings = 0;
    engine.setOutputWarningsToStandardError(false);
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&qmlWarnings](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings) {
            qWarning().noquote() << warning.toString();
            qmlWarnings++;
        }
    });

    // Only the first load is fatal: a broken edit during hot reload just waits for a fix.
    bool started = false;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [&started] {
            if (!started) {
                QCoreApplication::exit(EXIT_FAILURE);
            }
        },
        Qt::QueuedConnection);

#if defined(SCREENCAM_QML_SOURCE_DIR)
    QmlHotReloader hotReloader(&engine, QStringLiteral(SCREENCAM_QML_SOURCE_DIR),
                               QStringLiteral(SCREENCAM_HOT_RELOAD_DIR));
#endif
    FileOpenHandler fileOpenHandler(&engine);
    app.installEventFilter(&fileOpenHandler);
    QCoreApplication::processEvents(); // a Finder double-click may already be waiting
    if (initialProject.isEmpty()) {
        initialProject = fileOpenHandler.takePending();
    }

    engine.setInitialProperties({{QStringLiteral("initialProject"), initialProject}});
    engine.loadFromModule("ScreenCam", "Main");
    started = !engine.rootObjects().isEmpty();

    if (parser.isSet(selfTest)) {
        const QString screenshotPath = parser.value(screenshot);
        QTimer::singleShot(2500, &app, [&engine, &qmlWarnings, screenshotPath] {
            const bool ok = qmlWarnings == 0 && !engine.rootObjects().isEmpty();
            if (!screenshotPath.isEmpty() && !engine.rootObjects().isEmpty()) {
                if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
                    window->grabWindow().save(screenshotPath);
                }
            }
            qInfo().noquote() << (ok ? "self-test passed" : "self-test FAILED") << "-" << qmlWarnings
                              << "QML warning(s)";
            QCoreApplication::exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
        });
    }

    return QApplication::exec();
}
