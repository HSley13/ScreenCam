#pragma once

#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QVideoFrame>

#include <memory>

/// Captures a screen while leaving every ScreenCam window out of the picture.
///
/// Why: Qt's QScreenCapture on macOS records the screen through AVCaptureScreenInput, which
/// records *everything* on it — including ScreenCam's own preview, which then shows itself
/// inside itself forever (the "infinite mirror"), plus the floating bubble, controls and
/// outline. The per-window "don't share me" flag (NSWindowSharingNone) is not honoured by it.
/// Apple's ScreenCaptureKit can instead be told "this display, minus these apps", and the
/// exclusion covers every window the app has or opens later.
///
/// macOS 12.3+: ScreenCaptureKit (ExcludingScreenCapture_mac.mm). Elsewhere isSupported() is
/// false and CaptureSources keeps using QScreenCapture: on Windows our windows already hide
/// themselves from capture (WDA_EXCLUDEFROMCAPTURE); Linux has no way to do it.
class ExcludingScreenCapture : public QObject {
    Q_OBJECT

public:
    explicit ExcludingScreenCapture(QObject *parent = nullptr);
    ~ExcludingScreenCapture() override;

    static bool isSupported();

    void setScreen(QScreen *screen);
    QScreen *screen() const { return m_screen; }
    void setFramesPerSecond(int framesPerSecond);
    void setActive(bool active);
    bool isActive() const { return m_running; }

    struct Bridge;  // shared with the capture thread (see the .mm file)

signals:
    void frameArrived(const QVideoFrame &frame);
    void activeChanged();
    /// `permission`: true when macOS refused screen access (Screen Recording not allowed).
    void errorOccurred(const QString &message, bool permission);

private:
    void restart();
    void stopStream();
    void setRunning(bool running);

    struct Private;
    std::unique_ptr<Private> d;
    QPointer<QScreen> m_screen;
    int m_framesPerSecond = 30;
    bool m_wanted = false;   // setActive(true) was called
    bool m_running = false;  // frames are flowing
    quint64 m_generation = 0;  // bumps on every restart: late answers of old attempts are ignored
};
