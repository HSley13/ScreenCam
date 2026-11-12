// Entry point: create the application, pick a style, load the QML front end.
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlError>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

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
    parser.process(app);

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
    engine.loadFromModule("ScreenCam", "Main");

    if (parser.isSet(selfTest)) {
        QTimer::singleShot(2000, &app, [&engine, &qmlWarnings] {
            const bool ok = qmlWarnings == 0 && !engine.rootObjects().isEmpty();
            qInfo().noquote() << (ok ? "self-test passed" : "self-test FAILED") << "-" << qmlWarnings << "QML warning(s)";
            QCoreApplication::exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
        });
    }

    return QApplication::exec();
}
