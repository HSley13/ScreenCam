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

/// The record button's brain (QML singleton `Recorder`).
///
/// Screen + Camera mode records a *project* (screen.mp4 + camera.mp4 + project.json, see
/// Project) with two pipelines in parallel, so the camera bubble can be placed — and moved over
/// time — in the editor afterwards. Screen-only and Camera-only modes write a finished MP4.
///
///   Idle ──start()──► Countdown ──(n s)──► Recording ◄──togglePause()──► Paused
///     ▲                   │ cancel               │ stop()                  │ stop()
///     └───────────────────┴──────────── Finalizing ◄─────────────────────────┘
///
/// It snapshots the settings when recording begins, keeps the capture inputs alive while the
/// window is hidden, forwards live frames and bubble edits to the pipeline, and reports the
/// saved file.
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
    /// Plays a finished video, or opens a project in the editor.
    Q_INVOKABLE void openLastRecording();
    Q_INVOKABLE void clearError();
    /// Where the floating camera bubble's centre is on the desktop (global logical coords).
    Q_INVOKABLE void setBubbleDesktopCenter(const QPointF &center);
    /// That position as a normalized centre inside what is recorded (screen or region), to
    /// seed the editor's layout; a corner default when it isn't inside (or for windows).
    screencam::core::PointD bubbleCenterInTarget() const;

signals:
    void stateChanged();
    void countdownRemainingChanged();
    void elapsedChanged();
    void lastRecordingChanged();
    void errorStringChanged();
    /// The window should get out of the way (minimize) / come back.
    void windowHideRequested();
    void windowShowRequested();
    /// A project was recorded (or the user asked to reopen it): open it in the editor.
    void openProjectRequested(const QString &folder);

private:
    explicit RecordingController(QObject *parent = nullptr);

    QString validate() const;
    void beginRecording();
    void startPipelines();
    /// One pipeline closed its file (error empty) or failed.
    void onPipelineDone(bool camera, const QString &error, const QString &path);
    void completeProject();
    void finish();
    void setState(State state);
    void setError(const QString &message);
    QString nextOutputPath() const;
    QString nextProjectFolder() const;
    qint64 activeMilliseconds() const;

    RecordingPipeline m_pipeline;        // the video (direct modes) or screen.mp4 (projects)
    RecordingPipeline m_cameraPipeline;  // camera.mp4 (projects only)
    bool m_projectMode = false;
    bool m_startingPipelines = false;  // ignore synchronous failures while start() runs
    int m_pendingPipelines = 0;        // still writing their file
    bool m_screenSaved = false;
    bool m_cameraSaved = false;
    QString m_directPath;
    qint64 m_screenClockMs = -1;
    qint64 m_cameraClockMs = -1;
    qint64 m_recordedMs = 0;
    Project m_project;
    State m_state = State::Idle;
    QTimer m_countdownTimer;
    int m_countdownRemaining = 0;
    QTimer m_elapsedTicker;  // refreshes elapsedText a few times per second
    QElapsedTimer m_clock;
    qint64 m_pausedMs = 0;
    qint64 m_pauseStartedMs = -1;
    QString m_lastRecording;
    QString m_errorString;
    QPointF m_bubbleDesktopCenter{-1.0, -1.0};
};
