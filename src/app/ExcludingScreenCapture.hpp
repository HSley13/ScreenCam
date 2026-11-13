#pragma once

#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QVideoFrame>

#include <memory>

/// Captures a screen without any ScreenCam window. Qt's QScreenCapture on macOS records
/// everything, ScreenCam's own preview included (an endless mirror), and ignores
/// NSWindowSharingNone; ScreenCaptureKit can record "this display minus this app".
/// macOS 12.3+ only: elsewhere isSupported() is false and QScreenCapture is used (Windows
/// windows exclude themselves with WDA_EXCLUDEFROMCAPTURE; Linux can't).
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

    struct Bridge;  // shared with the capture thread

signals:
    void frameArrived(const QVideoFrame &frame);
    void activeChanged();
    /// `permission`: Screen Recording isn't allowed.
    void errorOccurred(const QString &message, bool permission);

private:
    void restart();
    void stopStream();
    void setRunning(bool running);

    struct Private;
    std::unique_ptr<Private> d;
    QPointer<QScreen> m_screen;
    int m_framesPerSecond = 30;
    bool m_wanted = false;
    bool m_running = false;
    quint64 m_generation = 0;  // bumped on restart: late answers of old attempts are ignored
};
