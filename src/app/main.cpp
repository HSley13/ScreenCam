// Entry point: create the application, pick a style, load the QML front end.
#include <QApplication>
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

    // "Basic" is the fully customizable Qt Quick Controls style: it looks identical on every OS,
    // whereas native styles (macOS, Windows) ignore most custom backgrounds/colours.
    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] {
            QCoreApplication::exit(EXIT_FAILURE);
        },
        Qt::QueuedConnection);
    engine.loadFromModule("ScreenCam", "Main");

    return QApplication::exec();
}
