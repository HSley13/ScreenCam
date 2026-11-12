// Linux/X11 part of the window picker: builds a fake desktop on a virtual X server (Xvfb)
// — an app window, a panel, a tiny tray icon, a skip-taskbar pop-up, a fixed splash and one
// of our own — and checks that only the app window is offered.
#include "app/AppWindows.hpp"

#include <QWindowCapture>
#include <QtMultimedia/private/qcapturablewindow_p.h>
#include <QtTest>

#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

class TestAppWindows : public QObject {
    Q_OBJECT

private:
    Display *m_display = nullptr;

    Atom atom(const char *name) { return XInternAtom(m_display, name, False); }

    Window makeWindow(int width, int height, const char *title, const char *appClass) {
        const Window window = XCreateSimpleWindow(m_display, DefaultRootWindow(m_display), 0, 0,
                                                  static_cast<unsigned>(width), static_cast<unsigned>(height), 0, 0, 0);
        XStoreName(m_display, window, title);
        XClassHint hint{};
        hint.res_name = const_cast<char *>(appClass);
        hint.res_class = const_cast<char *>(appClass);
        XSetClassHint(m_display, window, &hint);
        XMapWindow(m_display, window);  // shown, like real windows (Qt skips unmapped ones)
        return window;
    }

    void setAtoms(Window window, const char *property, std::initializer_list<const char *> names) {
        std::vector<Atom> atoms;
        for (const char *name : names) {
            atoms.push_back(atom(name));
        }
        XChangeProperty(m_display, window, atom(property), XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char *>(atoms.data()), static_cast<int>(atoms.size()));
    }

    void setPid(Window window, unsigned long pid) {
        XChangeProperty(m_display, window, atom("_NET_WM_PID"), XA_CARDINAL, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char *>(&pid), 1);
    }

private slots:
    void initTestCase() {
        // Qt only uses its X11 window list in an X11 *session* (it checks this variable);
        // under xvfb-run there is no session, so say it is one.
        qputenv("XDG_SESSION_TYPE", "x11");
        m_display = XOpenDisplay(nullptr);
        if (m_display == nullptr) {
            QSKIP("no X display (run through ctest, which starts Xvfb)");
        }
    }

    void cleanupTestCase() {
        if (m_display != nullptr) {
            XCloseDisplay(m_display);
        }
    }

    void listsOnlyRealAppWindows() {
        const Window editor = makeWindow(900, 600, "notes.txt", "TextEditor");
        setAtoms(editor, "_NET_WM_WINDOW_TYPE", {"_NET_WM_WINDOW_TYPE_NORMAL"});
        setAtoms(editor, "_NET_WM_ALLOWED_ACTIONS", {"_NET_WM_ACTION_MINIMIZE", "_NET_WM_ACTION_MAXIMIZE_HORZ"});
        setPid(editor, 1);

        const Window panel = makeWindow(1920, 32, "Top panel", "Panel");  // clock, battery ...
        setAtoms(panel, "_NET_WM_WINDOW_TYPE", {"_NET_WM_WINDOW_TYPE_DOCK"});

        const Window trayIcon = makeWindow(24, 24, "CPU", "CpuMeter");

        const Window popup = makeWindow(400, 300, "Volume", "Mixer");
        setAtoms(popup, "_NET_WM_STATE", {"_NET_WM_STATE_SKIP_TASKBAR"});

        const Window splash = makeWindow(500, 300, "Loading", "BigApp");
        setAtoms(splash, "_NET_WM_ALLOWED_ACTIONS", {"_NET_WM_ACTION_CLOSE"});  // can't min/max

        const Window ours = makeWindow(800, 600, "ScreenCam", "ScreenCam");
        setPid(ours, static_cast<unsigned long>(getpid()));

        // What a window manager publishes: the managed client windows.
        const Window clients[] = {editor, panel, trayIcon, popup, splash, ours};
        XChangeProperty(m_display, DefaultRootWindow(m_display), atom("_NET_CLIENT_LIST"), XA_WINDOW, 32,
                        PropModeReplace, reinterpret_cast<const unsigned char *>(clients), 6);
        XSync(m_display, False);

        bool supported = false;
        const QHash<quintptr, AppWindow> windows = listAppWindows(&supported);
        QVERIFY(supported);
        QCOMPARE(windows.size(), 1);
        QVERIFY(windows.contains(static_cast<quintptr>(editor)));
        const AppWindow &window = windows.value(static_cast<quintptr>(editor));
        QCOMPARE(window.appName, QStringLiteral("TextEditor"));
        QCOMPARE(window.title, QStringLiteral("notes.txt"));
        QCOMPARE(window.label, QStringLiteral("TextEditor — notes.txt"));

        // The picker matches our list with Qt's by native id: Qt's entry for the same
        // window must carry the X11 window id (that's what CaptureSources relies on).
        bool foundInQt = false;
        for (const QCapturableWindow &captured : QWindowCapture::capturableWindows()) {
            if (QCapturableWindowPrivate::handle(captured)->id == static_cast<QCapturableWindowPrivate::Id>(editor)) {
                foundInQt = true;
            }
        }
        QVERIFY(foundInQt);
    }
};

QTEST_MAIN(TestAppWindows)  // a QGuiApplication: Qt only lists X11 windows on the xcb platform
#include "tst_app_windows.moc"
