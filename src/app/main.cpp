// Entry point: create the application, pick a style, load the QML front end.
#include <QApplication>
#include <QFileOpenEvent>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlError>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>

namespace {

/// macOS hands a document double-clicked in Finder (a ".screencam" project) to the running
/// app as a QFileOpenEvent — also at launch, when the app was started by that double-click.
/// Forward it to Main.qml's openEditor(); keep it until the UI exists if it comes too early.
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

}  // namespace

int main(int argc, char *argv[]) {
    // QApplication rather than QGuiApplication: the tray icon (Qt.labs.platform) falls back to
    // Qt Widgets on Windows and Linux, which requires a QApplication instance.
    QApplication app(argc, argv);
    QApplication::setOrganizationName("hsley13");
    QApplication::setOrganizationDomain("hsley13.github.io");
    QApplication::setApplicationName("ScreenCam");
    QApplication::setApplicationDisplayName("ScreenCam");
    QApplication::setApplicationVersion(SCREENCAM_VERSION);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/screencam/screencam.png")));

    // "Basic" is the fully customizable Qt Quick Controls style: it looks identical on every OS,
    // whereas native styles (macOS, Windows) ignore most custom backgrounds/colours.
    QQuickStyle::setStyle("Basic");

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Screen recorder with a draggable camera bubble"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption selfTest(
        QStringLiteral("self-test"),
        QStringLiteral("Load the UI, run for two seconds and exit; non-zero if QML reported any warning (used by CTest/CI)."));
    parser.addOption(selfTest);
    const QCommandLineOption screenshot(
        QStringLiteral("screenshot"),
        QStringLiteral("With --self-test: save a picture of the window to <file> before exiting."),
        QStringLiteral("file"));
    parser.addOption(screenshot);
    const QCommandLineOption openProject(
        QStringLiteral("open"), QStringLiteral("Open a .screencam project in the editor."), QStringLiteral("project"));
    parser.addOption(openProject);
    // A bare path works too (what the OS passes when a project folder is dropped on the app).
    parser.addPositionalArgument(QStringLiteral("project"), QStringLiteral("Optional .screencam project to open."));
    parser.process(app);

    QString initialProject = parser.value(openProject);
    if (initialProject.isEmpty() && !parser.positionalArguments().isEmpty()) {
        initialProject = parser.positionalArguments().constFirst();
    }

    QQmlApplicationEngine engine;

    // Count QML warnings (binding errors, missing types, ...) so --self-test can fail on them.
    int qmlWarnings = 0;
    engine.setOutputWarningsToStandardError(false);
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&qmlWarnings](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings) {
            qWarning().noquote() << warning.toString();
            qmlWarnings++;
        }
    });

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] {
            QCoreApplication::exit(EXIT_FAILURE);
        },
        Qt::QueuedConnection);
    FileOpenHandler fileOpenHandler(&engine);
    app.installEventFilter(&fileOpenHandler);
    QCoreApplication::processEvents();  // a Finder double-click may already be waiting
    if (initialProject.isEmpty()) {
        initialProject = fileOpenHandler.takePending();
    }

    // Main.qml declares `initialProject`; it switches to the editor when it's set.
    engine.setInitialProperties({{QStringLiteral("initialProject"), initialProject}});
    engine.loadFromModule("ScreenCam", "Main");

    if (parser.isSet(selfTest)) {
        const QString screenshotPath = parser.value(screenshot);
        QTimer::singleShot(2500, &app, [&engine, &qmlWarnings, screenshotPath] {
            const bool ok = qmlWarnings == 0 && !engine.rootObjects().isEmpty();
            if (!screenshotPath.isEmpty() && !engine.rootObjects().isEmpty()) {
                if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
                    window->grabWindow().save(screenshotPath);
                }
            }
            qInfo().noquote() << (ok ? "self-test passed" : "self-test FAILED") << "-" << qmlWarnings << "QML warning(s)";
            QCoreApplication::exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
        });
    }

    return QApplication::exec();
}
