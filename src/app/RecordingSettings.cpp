#include "app/RecordingSettings.hpp"

#include "app/DeviceCatalog.hpp"
#include "core/Resolution.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QJSEngine>
#include <QMediaDevices>
#include <QStandardPaths>

#include <algorithm>
#include <type_traits>

namespace {

QString defaultOutputFolder() {
    QString movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (movies.isEmpty()) {
        movies = QDir::homePath();
    }
    return QDir(movies).filePath(QStringLiteral("ScreenCam"));
}

/// Stores enums as ints in QSettings.
template <typename T>
QVariant toStored(const T &value) {
    if constexpr (std::is_enum_v<T>) {
        return static_cast<int>(value);
    } else {
        return QVariant::fromValue(value);
    }
}

}  // namespace

RecordingSettings *RecordingSettings::instance() {
    static RecordingSettings *settings = new RecordingSettings(qApp);
    return settings;
}

RecordingSettings *RecordingSettings::create(QQmlEngine *, QJSEngine *) {
    RecordingSettings *settings = instance();
    QJSEngine::setObjectOwnership(settings, QJSEngine::CppOwnership);
    return settings;
}

RecordingSettings::RecordingSettings(QObject *parent)
    : QObject(parent) {
    // The system defaults on first launch, then what was saved.
    const QString defaultCamera = DeviceCatalog::idOf(QMediaDevices::defaultVideoInput());
    const QString defaultMicrophone = DeviceCatalog::idOf(QMediaDevices::defaultAudioInput());

    m_screenId = m_store.value("screen", DeviceCatalog::idOf(QGuiApplication::primaryScreen())).toString();
    m_captureTarget = static_cast<CaptureTarget>(m_store.value("captureTarget", static_cast<int>(m_captureTarget)).toInt());
    m_region = m_store.value("region", QRect()).toRect();
    m_cameraId = m_store.value("camera", defaultCamera).toString();
    if (m_cameraId.isEmpty()) {
        m_cameraId = defaultCamera;  // never start without a camera
    }
    m_microphoneId = m_store.value("microphone", defaultMicrophone).toString();
    m_resolution = m_store.value("resolution", m_resolution).toInt();
    m_frameRate = m_store.value("frameRate", m_frameRate).toInt();
    m_countdownSeconds = m_store.value("countdown", m_countdownSeconds).toInt();
    m_hideWindowWhileRecording = m_store.value("hideWindow", m_hideWindowWhileRecording).toBool();
    m_showPointer = m_store.value("showPointer", m_showPointer).toBool();
    m_highlightPointer = m_store.value("highlightPointer", m_highlightPointer).toBool();
    m_outputFolder = m_store.value("outputFolder", defaultOutputFolder()).toString();
}

template <typename T>
void RecordingSettings::assign(T &field, const T &value, const char *key, void (RecordingSettings::*changed)()) {
    if (field == value) {
        return;
    }
    field = value;
    m_store.setValue(QLatin1StringView(key), toStored(value));
    emit(this->*changed)();
}

void RecordingSettings::setScreenId(const QString &id) {
    assign(m_screenId, id, "screen", &RecordingSettings::screenIdChanged);
}

void RecordingSettings::setCaptureTarget(CaptureTarget target) {
    assign(m_captureTarget, target, "captureTarget", &RecordingSettings::captureTargetChanged);
}

void RecordingSettings::setRegion(const QRect &region) {
    assign(m_region, region.normalized(), "region", &RecordingSettings::regionChanged);
}

void RecordingSettings::setCameraId(const QString &id) {
    assign(m_cameraId, id, "camera", &RecordingSettings::cameraIdChanged);
}

void RecordingSettings::setMicrophoneId(const QString &id) {
    assign(m_microphoneId, id, "microphone", &RecordingSettings::microphoneIdChanged);
}

void RecordingSettings::setResolution(int preset) {
    assign(m_resolution, preset, "resolution", &RecordingSettings::resolutionChanged);
}

void RecordingSettings::setFrameRate(int fps) {
    assign(m_frameRate, std::clamp(fps, 1, 60), "frameRate", &RecordingSettings::frameRateChanged);
}

void RecordingSettings::setCountdownSeconds(int seconds) {
    assign(m_countdownSeconds, std::clamp(seconds, 0, 10), "countdown", &RecordingSettings::countdownSecondsChanged);
}

void RecordingSettings::setHideWindowWhileRecording(bool hide) {
    assign(m_hideWindowWhileRecording, hide, "hideWindow", &RecordingSettings::hideWindowWhileRecordingChanged);
}

void RecordingSettings::setShowPointer(bool show) {
    assign(m_showPointer, show, "showPointer", &RecordingSettings::showPointerChanged);
}

void RecordingSettings::setHighlightPointer(bool highlight) {
    assign(m_highlightPointer, highlight, "highlightPointer", &RecordingSettings::highlightPointerChanged);
}

QUrl RecordingSettings::outputFolderUrl() const {
    return QUrl::fromLocalFile(m_outputFolder);
}

void RecordingSettings::setOutputFolderUrl(const QUrl &url) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (!path.isEmpty()) {
        assign(m_outputFolder, path, "outputFolder", &RecordingSettings::outputFolderChanged);
    }
}

QVariantList RecordingSettings::resolutionOptions() const {
    QVariantList list;
    for (const auto &option : screencam::core::resolutionOptions()) {
        list.append(QVariantMap{
            {QStringLiteral("value"), static_cast<int>(option.preset)},
            {QStringLiteral("label"), QString::fromUtf8(option.label.data(), static_cast<qsizetype>(option.label.size()))},
        });
    }
    return list;
}

void RecordingSettings::openOutputFolder() const {
    QDir().mkpath(m_outputFolder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_outputFolder));
}
