// Platforms without app-excluding screen capture: CaptureSources uses QScreenCapture there.
#include "app/ExcludingScreenCapture.hpp"

struct ExcludingScreenCapture::Private {};
struct ExcludingScreenCapture::Bridge {};

ExcludingScreenCapture::ExcludingScreenCapture(QObject *parent)
    : QObject(parent) {
}

ExcludingScreenCapture::~ExcludingScreenCapture() = default;

bool ExcludingScreenCapture::isSupported() {
    return false;
}

void ExcludingScreenCapture::setScreen(QScreen *screen) {
    m_screen = screen;
}

void ExcludingScreenCapture::setFramesPerSecond(int framesPerSecond) {
    m_framesPerSecond = framesPerSecond;
}

void ExcludingScreenCapture::setActive(bool active) {
    m_wanted = active;
}

void ExcludingScreenCapture::restart() {
}

void ExcludingScreenCapture::stopStream() {
}

void ExcludingScreenCapture::setRunning(bool running) {
    m_running = running;
}
