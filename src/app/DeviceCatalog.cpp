#include "app/DeviceCatalog.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJSEngine>
#include <QPermissions>
#include <QScreen>

namespace {

QVariantMap entry(const QString &id, const QString &name) {
    return QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}};
}

bool permissionGranted(const QPermission &permission) {
    return qApp->checkPermission(permission) == Qt::PermissionStatus::Granted;
}

}  // namespace

DeviceCatalog *DeviceCatalog::instance() {
    static DeviceCatalog *catalog = new DeviceCatalog(qApp);  // lives as long as the app
    return catalog;
}

DeviceCatalog *DeviceCatalog::create(QQmlEngine *, QJSEngine *) {
    DeviceCatalog *catalog = instance();
    // The engine must not delete an object it didn't create.
    QJSEngine::setObjectOwnership(catalog, QJSEngine::CppOwnership);
    return catalog;
}

DeviceCatalog::DeviceCatalog(QObject *parent)
    : QObject(parent) {
    connect(&m_mediaDevices, &QMediaDevices::videoInputsChanged, this, &DeviceCatalog::camerasChanged);
    connect(&m_mediaDevices, &QMediaDevices::audioInputsChanged, this, &DeviceCatalog::microphonesChanged);

    auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    if (app != nullptr) {
        connect(app, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
            watchScreen(screen);
            emit screensChanged();
        });
        connect(app, &QGuiApplication::screenRemoved, this, &DeviceCatalog::screensChanged);
        connect(app, &QGuiApplication::primaryScreenChanged, this, &DeviceCatalog::screensChanged);
        for (QScreen *screen : QGuiApplication::screens()) {
            watchScreen(screen);
        }
    }
}

void DeviceCatalog::watchScreen(QScreen *screen) {
    // Resolution or scaling changes alter the pixel size shown in the list.
    connect(screen, &QScreen::geometryChanged, this, &DeviceCatalog::screensChanged);
    connect(screen, &QScreen::physicalDotsPerInchChanged, this, &DeviceCatalog::screensChanged);
}

QVariantList DeviceCatalog::cameras() const {
    QVariantList list;  // no "No camera": every recording has you in it
    for (const QCameraDevice &device : QMediaDevices::videoInputs()) {
        list.append(entry(idOf(device), device.description()));
    }
    return list;
}

QVariantList DeviceCatalog::microphones() const {
    QVariantList list{entry(QString(), tr("No microphone"))};
    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
        list.append(entry(idOf(device), device.description()));
    }
    return list;
}

QVariantList DeviceCatalog::screens() const {
    QVariantList list;
    const QList<QScreen *> all = QGuiApplication::screens();
    for (int i = 0; i < all.size(); i++) {
        const QScreen *screen = all.at(i);
        const QSize pixels = screenPixelSize(screen);
        const QString label = QStringLiteral("%1 — %2 × %3")
                                  .arg(screen->name().isEmpty() ? tr("Screen %1").arg(i + 1) : screen->name())
                                  .arg(pixels.width())
                                  .arg(pixels.height());
        QVariantMap map = entry(idOf(screen), label);
        map.insert(QStringLiteral("width"), pixels.width());
        map.insert(QStringLiteral("height"), pixels.height());
        // Logical (point) geometry on the virtual desktop: where the region selector opens.
        map.insert(QStringLiteral("geometry"), screen->geometry());
        map.insert(QStringLiteral("pixelRatio"), screen->devicePixelRatio());
        list.append(map);
    }
    return list;
}

bool DeviceCatalog::cameraAllowed() const {
    return permissionGranted(QCameraPermission{});
}

bool DeviceCatalog::microphoneAllowed() const {
    return permissionGranted(QMicrophonePermission{});
}

void DeviceCatalog::requestPermissions() {
    // Ask one at a time so macOS shows one dialog after the other.
    qApp->requestPermission(QCameraPermission{}, this, [this](const QPermission &) {
        qApp->requestPermission(QMicrophonePermission{}, this, [this](const QPermission &) {
            emit permissionsChanged();
            // Device lists can be empty until access is granted.
            emit camerasChanged();
            emit microphonesChanged();
        });
    });
}

QString DeviceCatalog::idOf(const QCameraDevice &device) {
    return QString::fromUtf8(device.id());
}

QString DeviceCatalog::idOf(const QAudioDevice &device) {
    return QString::fromUtf8(device.id());
}

QString DeviceCatalog::idOf(const QScreen *screen) {
    return (screen != nullptr) ? screen->name() : QString();
}

std::optional<QCameraDevice> DeviceCatalog::findCamera(const QString &id) {
    if (id.isEmpty()) {
        return std::nullopt;
    }
    for (const QCameraDevice &device : QMediaDevices::videoInputs()) {
        if (idOf(device) == id) {
            return device;
        }
    }
    return std::nullopt;
}

std::optional<QAudioDevice> DeviceCatalog::findMicrophone(const QString &id) {
    if (id.isEmpty()) {
        return std::nullopt;
    }
    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
        if (idOf(device) == id) {
            return device;
        }
    }
    return std::nullopt;
}

QScreen *DeviceCatalog::findScreen(const QString &id) {
    for (QScreen *screen : QGuiApplication::screens()) {
        if (idOf(screen) == id) {
            return screen;
        }
    }
    return QGuiApplication::primaryScreen();
}

QSize DeviceCatalog::screenPixelSize(const QScreen *screen) {
    if (screen == nullptr) {
        return {};
    }
    const qreal ratio = screen->devicePixelRatio();
    const QSize logical = screen->geometry().size();
    return {qRound(logical.width() * ratio), qRound(logical.height() * ratio)};
}
