#pragma once

#include <QObject>
#include <QSettings>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/// Everything the user picks in the sidebar, saved automatically (QSettings) so the app
/// opens the way you left it. QML singleton `RecordingSettings`; C++ uses instance().
class RecordingSettings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(CaptureMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString screenId READ screenId WRITE setScreenId NOTIFY screenIdChanged)
    Q_PROPERTY(QString cameraId READ cameraId WRITE setCameraId NOTIFY cameraIdChanged)
    Q_PROPERTY(QString microphoneId READ microphoneId WRITE setMicrophoneId NOTIFY microphoneIdChanged)
    Q_PROPERTY(int resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(int frameRate READ frameRate WRITE setFrameRate NOTIFY frameRateChanged)
    Q_PROPERTY(int countdownSeconds READ countdownSeconds WRITE setCountdownSeconds NOTIFY countdownSecondsChanged)
    Q_PROPERTY(bool hideWindowWhileRecording READ hideWindowWhileRecording WRITE setHideWindowWhileRecording NOTIFY
                   hideWindowWhileRecordingChanged)
    Q_PROPERTY(Orientation cameraOnlyOrientation READ cameraOnlyOrientation WRITE setCameraOnlyOrientation NOTIFY
                   cameraOnlyOrientationChanged)
    Q_PROPERTY(QString outputFolder READ outputFolder NOTIFY outputFolderChanged)
    Q_PROPERTY(QUrl outputFolderUrl READ outputFolderUrl WRITE setOutputFolderUrl NOTIFY outputFolderChanged)

public:
    /// What ends up in the video.
    enum class CaptureMode {
        ScreenAndCamera,  // screen + camera bubble
        ScreenOnly,
        CameraOnly,       // just you, in landscape or portrait
    };
    Q_ENUM(CaptureMode)

    /// Mirrors screencam::core::CameraOrientation (same values) for QML.
    enum class Orientation {
        Landscape,
        Portrait,
    };
    Q_ENUM(Orientation)

    static RecordingSettings *instance();
    static RecordingSettings *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    CaptureMode mode() const { return m_mode; }
    void setMode(CaptureMode mode);
    QString screenId() const { return m_screenId; }
    void setScreenId(const QString &id);
    QString cameraId() const { return m_cameraId; }
    void setCameraId(const QString &id);
    QString microphoneId() const { return m_microphoneId; }
    void setMicrophoneId(const QString &id);
    int resolution() const { return m_resolution; }  // a screencam::core::ResolutionPreset value
    void setResolution(int preset);
    int frameRate() const { return m_frameRate; }
    void setFrameRate(int fps);
    int countdownSeconds() const { return m_countdownSeconds; }
    void setCountdownSeconds(int seconds);
    bool hideWindowWhileRecording() const { return m_hideWindowWhileRecording; }
    void setHideWindowWhileRecording(bool hide);
    Orientation cameraOnlyOrientation() const { return m_cameraOnlyOrientation; }
    void setCameraOnlyOrientation(Orientation orientation);
    QString outputFolder() const { return m_outputFolder; }
    QUrl outputFolderUrl() const;
    void setOutputFolderUrl(const QUrl &url);

    bool usesScreen() const { return m_mode != CaptureMode::CameraOnly; }
    bool usesCamera() const { return m_mode != CaptureMode::ScreenOnly && !m_cameraId.isEmpty(); }

    /// [{ value: <preset>, label: "1080p" }, ...] for the resolution picker.
    Q_INVOKABLE QVariantList resolutionOptions() const;
    /// Opens the output folder in Finder / Explorer / the file manager.
    Q_INVOKABLE void openOutputFolder() const;

signals:
    void modeChanged();
    void screenIdChanged();
    void cameraIdChanged();
    void microphoneIdChanged();
    void resolutionChanged();
    void frameRateChanged();
    void countdownSecondsChanged();
    void hideWindowWhileRecordingChanged();
    void cameraOnlyOrientationChanged();
    void outputFolderChanged();

private:
    explicit RecordingSettings(QObject *parent = nullptr);

    /// Shared setter body: ignore no-op writes, persist, notify QML.
    template <typename T>
    void assign(T &field, const T &value, const char *key, void (RecordingSettings::*changed)());

    QSettings m_store;
    CaptureMode m_mode = CaptureMode::ScreenAndCamera;
    QString m_screenId;
    QString m_cameraId;
    QString m_microphoneId;
    int m_resolution = 1;  // FullHD1080
    int m_frameRate = 30;
    int m_countdownSeconds = 3;
    bool m_hideWindowWhileRecording = true;
    Orientation m_cameraOnlyOrientation = Orientation::Landscape;
    QString m_outputFolder;
};
