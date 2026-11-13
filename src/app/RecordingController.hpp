#pragma once

#include "core/BubbleGeometry.hpp"
#include "media/Project.hpp"
#include "media/RecordingPipeline.hpp"

#include <QElapsedTimer>
#include <QPointF>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/// Runs a recording (QML singleton `Recorder`). It records a project: screen.mp4 and
/// camera.mp4 with two pipelines in parallel, so the bubble can be placed in the editor.
///
///   Idle ──start()──► Countdown ──(n s)──► Recording ◄──togglePause()──► Paused
///     ▲                   │ cancel               │ stop()                  │ stop()
///     └───────────────────┴──────────── Finalizing ◄─────────────────────────┘
class RecordingController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Recorder)
    QML_SINGLETON

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(int countdownRemaining READ countdownRemaining NOTIFY countdownRemainingChanged)
    Q_PROPERTY(QString elapsedText READ elapsedText NOTIFY elapsedChanged)
    Q_PROPERTY(QString lastRecording READ lastRecording NOTIFY lastRecordingChanged)
    Q_PROPERTY(QString lastRecordingName READ lastRecordingName NOTIFY lastRecordingChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    enum class State { Idle, Countdown, Recording, Paused, Finalizing };
    Q_ENUM(State)

    static RecordingController *instance();
    static RecordingController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    State state() const { return m_state; }
    bool busy() const { return m_state != State::Idle; }
    int countdownRemaining() const { return m_countdownRemaining; }
    QString elapsedText() const;
    QString lastRecording() const { return m_lastRecording; }
    QString lastRecordingName() const;
    QString errorString() const { return m_errorString; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void toggleRecording();  // start, or stop if already going
    Q_INVOKABLE void cancelCountdown();
    Q_INVOKABLE void openLastRecording();
    Q_INVOKABLE void clearError();
    /// The floating bubble's centre on the desktop.
    Q_INVOKABLE void setBubbleDesktopCenter(const QPointF &center);
    /// That centre inside what is recorded, to seed the editor's layout (a corner otherwise).
    screencam::core::PointD bubbleCenterInTarget() const;

signals:
    void stateChanged();
    void countdownRemainingChanged();
    void elapsedChanged();
    void lastRecordingChanged();
    void errorStringChanged();
    void windowHideRequested();
    void windowShowRequested();
    void openProjectRequested(const QString &folder);

private:
    explicit RecordingController(QObject *parent = nullptr);

    QString validate() const;
    void beginRecording();
    void startPipelines();
    void onPipelineDone(bool camera, const QString &error);
    void completeProject();
    void finish();
    void setState(State state);
    void setError(const QString &message);
    QString nextProjectFolder() const;
    qint64 activeMilliseconds() const;

    RecordingPipeline m_pipeline;        // screen.mp4
    RecordingPipeline m_cameraPipeline;  // camera.mp4
    bool m_startingPipelines = false;  // ignore synchronous failures while start() runs
    int m_pendingPipelines = 0;
    bool m_screenSaved = false;
    bool m_cameraSaved = false;
    qint64 m_screenClockMs = -1;
    qint64 m_cameraClockMs = -1;
    qint64 m_recordedMs = 0;
    Project m_project;
    State m_state = State::Idle;
    QTimer m_countdownTimer;
    int m_countdownRemaining = 0;
    QTimer m_pointerTicker;  // ~60x/s
    QTimer m_elapsedTicker;
    QElapsedTimer m_clock;
    qint64 m_pausedMs = 0;
    qint64 m_pauseStartedMs = -1;
    QString m_lastRecording;
    QString m_errorString;
    QPointF m_bubbleDesktopCenter{-1.0, -1.0};
};
