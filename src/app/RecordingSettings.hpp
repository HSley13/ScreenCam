#pragma once

#include <QObject>
#include <QRect>
#include <QSettings>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/// The sidebar's choices, kept in QSettings (QML singleton `RecordingSettings`).
class RecordingSettings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString screenId READ screenId WRITE setScreenId NOTIFY screenIdChanged)
    Q_PROPERTY(CaptureTarget captureTarget READ captureTarget WRITE setCaptureTarget NOTIFY captureTargetChanged)
    // In the selected screen's logical coordinates; empty until chosen.
    Q_PROPERTY(QRect region READ region WRITE setRegion NOTIFY regionChanged)
    Q_PROPERTY(QString cameraId READ cameraId WRITE setCameraId NOTIFY cameraIdChanged)
    Q_PROPERTY(QString microphoneId READ microphoneId WRITE setMicrophoneId NOTIFY microphoneIdChanged)
    Q_PROPERTY(int resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(int frameRate READ frameRate WRITE setFrameRate NOTIFY frameRateChanged)
    Q_PROPERTY(int countdownSeconds READ countdownSeconds WRITE setCountdownSeconds NOTIFY countdownSecondsChanged)
    Q_PROPERTY(bool hideWindowWhileRecording READ hideWindowWhileRecording WRITE setHideWindowWhileRecording NOTIFY
                   hideWindowWhileRecordingChanged)
    Q_PROPERTY(bool showPointer READ showPointer WRITE setShowPointer NOTIFY showPointerChanged)
    Q_PROPERTY(bool highlightPointer READ highlightPointer WRITE setHighlightPointer NOTIFY highlightPointerChanged)
    Q_PROPERTY(QString outputFolder READ outputFolder NOTIFY outputFolderChanged)
    Q_PROPERTY(QUrl outputFolderUrl READ outputFolderUrl WRITE setOutputFolderUrl NOTIFY outputFolderChanged)

public:
    enum class CaptureTarget {
        Screen,
        Window,
        Region,  // a rectangle dragged out on a display
    };
    Q_ENUM(CaptureTarget)

    static RecordingSettings *instance();
    static RecordingSettings *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QString screenId() const { return m_screenId; }
    void setScreenId(const QString &id);
    CaptureTarget captureTarget() const { return m_captureTarget; }
    void setCaptureTarget(CaptureTarget target);
    QRect region() const { return m_region; }
    void setRegion(const QRect &region);
    QString cameraId() const { return m_cameraId; }
    void setCameraId(const QString &id);
    QString microphoneId() const { return m_microphoneId; }
    void setMicrophoneId(const QString &id);
    int resolution() const { return m_resolution; }  // a core::ResolutionPreset
    void setResolution(int preset);
    int frameRate() const { return m_frameRate; }
    void setFrameRate(int fps);
    int countdownSeconds() const { return m_countdownSeconds; }
    void setCountdownSeconds(int seconds);
    bool hideWindowWhileRecording() const { return m_hideWindowWhileRecording; }
    void setHideWindowWhileRecording(bool hide);
    bool showPointer() const { return m_showPointer; }
    void setShowPointer(bool show);
    bool highlightPointer() const { return m_highlightPointer; }
    void setHighlightPointer(bool highlight);
    QString outputFolder() const { return m_outputFolder; }
    QUrl outputFolderUrl() const;
    void setOutputFolderUrl(const QUrl &url);

    /// [{ value: <preset>, label: "1080p" }, ...]
    Q_INVOKABLE QVariantList resolutionOptions() const;
    Q_INVOKABLE void openOutputFolder() const;

signals:
    void screenIdChanged();
    void captureTargetChanged();
    void regionChanged();
    void cameraIdChanged();
    void microphoneIdChanged();
    void resolutionChanged();
    void frameRateChanged();
    void countdownSecondsChanged();
    void hideWindowWhileRecordingChanged();
    void showPointerChanged();
    void highlightPointerChanged();
    void outputFolderChanged();

private:
    explicit RecordingSettings(QObject *parent = nullptr);

    /// Shared setter body: ignore no-op writes, persist, notify QML.
    template <typename T>
    void assign(T &field, const T &value, const char *key, void (RecordingSettings::*changed)());

    QSettings m_store;
    QString m_screenId;
    CaptureTarget m_captureTarget = CaptureTarget::Screen;
    QRect m_region;
    QString m_cameraId;
    QString m_microphoneId;
    int m_resolution = 1;  // 1080p
    int m_frameRate = 30;
    int m_countdownSeconds = 3;
    bool m_hideWindowWhileRecording = true;
    bool m_showPointer = true;
    bool m_highlightPointer = false;
    QString m_outputFolder;
};
