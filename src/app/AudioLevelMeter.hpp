#pragma once

#include <QAudioSource>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <memory>

/// Live input level of one microphone, for the meter under the microphone picker.
/// Usage in QML:  AudioLevelMeter { deviceId: RecordingSettings.microphoneId; active: true }
///                ... width: meter.level * track.width
class AudioLevelMeter : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString deviceId READ deviceId WRITE setDeviceId NOTIFY deviceIdChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(double level READ level NOTIFY levelChanged)  // 0..1 on a dB scale

public:
    explicit AudioLevelMeter(QObject *parent = nullptr);
    ~AudioLevelMeter() override;

    QString deviceId() const { return m_deviceId; }
    void setDeviceId(const QString &id);
    bool active() const { return m_active; }
    void setActive(bool active);
    double level() const { return m_level; }

signals:
    void deviceIdChanged();
    void activeChanged();
    void levelChanged();

private:
    void restart();
    void stop();
    void readSamples();
    void setLevel(double level);

    QString m_deviceId;
    bool m_active = false;
    double m_level = 0.0;
    std::unique_ptr<QAudioSource> m_source;
    QIODevice *m_io = nullptr;  // owned by m_source
    QTimer m_decayTimer;        // lets the bar fall when no audio arrives
};
