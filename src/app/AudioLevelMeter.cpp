#include "app/AudioLevelMeter.hpp"

#include "app/DeviceCatalog.hpp"
#include "core/AudioLevel.hpp"

#include <QAudioFormat>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kDecayPerTick = 0.18;
constexpr int kDecayIntervalMs = 50;
}  // namespace

AudioLevelMeter::AudioLevelMeter(QObject *parent)
    : QObject(parent) {
    m_decayTimer.setInterval(kDecayIntervalMs);
    connect(&m_decayTimer, &QTimer::timeout, this, [this] {
        setLevel(screencam::core::smoothMeterLevel(m_level, 0.0, kDecayPerTick));
    });
}

AudioLevelMeter::~AudioLevelMeter() {
    stop();
}

void AudioLevelMeter::setDeviceId(const QString &id) {
    if (m_deviceId == id) {
        return;
    }
    m_deviceId = id;
    emit deviceIdChanged();
    restart();
}

void AudioLevelMeter::setActive(bool active) {
    if (m_active == active) {
        return;
    }
    m_active = active;
    emit activeChanged();
    restart();
}

void AudioLevelMeter::stop() {
    m_decayTimer.stop();
    if (m_source != nullptr) {
        m_source->stop();
        m_source.reset();
    }
    m_io = nullptr;
    setLevel(0.0);
}

void AudioLevelMeter::restart() {
    stop();
    if (!m_active) {
        return;
    }
    const std::optional<QAudioDevice> device = DeviceCatalog::findMicrophone(m_deviceId);
    if (!device) {
        return;
    }
    // No resampling just for a meter.
    m_source = std::make_unique<QAudioSource>(*device, device->preferredFormat());
    m_source->setBufferSize(4096);
    m_io = m_source->start();  // push mode: readyRead as samples arrive
    if (m_io == nullptr) {
        m_source.reset();
        return;
    }
    connect(m_io, &QIODevice::readyRead, this, &AudioLevelMeter::readSamples);
    m_decayTimer.start();
}

void AudioLevelMeter::readSamples() {
    if (m_io == nullptr || m_source == nullptr) {
        return;
    }
    const QByteArray data = m_io->readAll();
    const QAudioFormat format = m_source->format();
    const int bytesPerSample = format.bytesPerSample();
    if (bytesPerSample <= 0) {
        return;
    }

    // normalizedSampleValue() handles every sample format and returns -1..1.
    double peak = 0.0;
    for (qsizetype offset = 0; offset + bytesPerSample <= data.size(); offset += bytesPerSample) {
        peak = std::max(peak, static_cast<double>(std::abs(format.normalizedSampleValue(data.constData() + offset))));
    }
    setLevel(screencam::core::smoothMeterLevel(m_level, screencam::core::peakToMeterLevel(peak), kDecayPerTick));
}

void AudioLevelMeter::setLevel(double level) {
    if (std::abs(level - m_level) < 0.001) {
        return;
    }
    m_level = level;
    emit levelChanged();
}
