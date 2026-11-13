// ScreenCaptureKit: "this display, without ScreenCam's windows". Compiled with ARC.
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "app/ExcludingScreenCapture.hpp"

#include <QAbstractVideoBuffer>
#include <QVideoFrameFormat>

#include <algorithm>
#include <memory>
#include <mutex>

#include <unistd.h>

namespace {

/// Zero-copy: a frame reads straight from ScreenCaptureKit's pixel buffer, kept alive while
/// any QVideoFrame holds it.
class PixelBufferVideoBuffer final : public QAbstractVideoBuffer {
public:
    explicit PixelBufferVideoBuffer(CVPixelBufferRef buffer)
        : m_buffer(CVPixelBufferRetain(buffer))
        , m_format(QSize(static_cast<int>(CVPixelBufferGetWidth(buffer)), static_cast<int>(CVPixelBufferGetHeight(buffer))),
                   QVideoFrameFormat::Format_BGRA8888) {}  // kCVPixelFormatType_32BGRA

    ~PixelBufferVideoBuffer() override {
        unmap();
        CVPixelBufferRelease(m_buffer);
    }

    MapData map(QVideoFrame::MapMode mode) override {
        MapData data;
        if (mode != QVideoFrame::ReadOnly || m_locked) {
            return data;  // read-only
        }
        if (CVPixelBufferLockBaseAddress(m_buffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) {
            return data;
        }
        m_locked = true;
        data.planeCount = 1;
        data.bytesPerLine[0] = static_cast<int>(CVPixelBufferGetBytesPerRow(m_buffer));
        data.data[0] = static_cast<uchar *>(CVPixelBufferGetBaseAddress(m_buffer));
        data.dataSize[0] = data.bytesPerLine[0] * static_cast<int>(CVPixelBufferGetHeight(m_buffer));
        return data;
    }

    void unmap() override {
        if (m_locked) {
            CVPixelBufferUnlockBaseAddress(m_buffer, kCVPixelBufferLock_ReadOnly);
            m_locked = false;
        }
    }

    QVideoFrameFormat format() const override { return m_format; }

private:
    CVPixelBufferRef m_buffer;
    QVideoFrameFormat m_format;
    bool m_locked = false;
};

/// QScreen doesn't expose its display id: match by position (Quartz and Qt agree on it).
CGDirectDisplayID displayIdOf(const QScreen *screen) {
    CGDirectDisplayID displays[16];
    uint32_t count = 0;
    if (screen == nullptr || CGGetActiveDisplayList(16, displays, &count) != kCGErrorSuccess) {
        return CGMainDisplayID();
    }
    const QRect geometry = screen->geometry();
    for (uint32_t i = 0; i < count; i++) {
        const CGRect bounds = CGDisplayBounds(displays[i]);
        if (qRound(bounds.origin.x) == geometry.x() && qRound(bounds.origin.y) == geometry.y() &&
            qRound(bounds.size.width) == geometry.width() && qRound(bounds.size.height) == geometry.height()) {
            return displays[i];
        }
    }
    return CGMainDisplayID();
}

}  // namespace

/// How ScreenCaptureKit's threads reach the C++ object. It can outlive the object (a frame
/// may be in flight after stop), so the pointer is cleared under a lock, and answers for an
/// older attempt (generation) are dropped.
struct ExcludingScreenCapture::Bridge {
    std::mutex mutex;
    ExcludingScreenCapture *owner = nullptr;

    // Runs `fn(owner)` on the owner's thread, if it still exists and is still on `generation`.
    template <typename Fn>
    void post(quint64 generation, Fn fn) {
        std::lock_guard<std::mutex> lock(mutex);
        if (owner == nullptr) {
            return;
        }
        ExcludingScreenCapture *target = owner;
        QMetaObject::invokeMethod(
            target,
            [target, generation, fn = std::move(fn)]() mutable {
                if (target->m_generation == generation) {
                    fn(target);
                }
            },
            Qt::QueuedConnection);
    }

    void deliverFrame(quint64 generation, const QVideoFrame &frame) {
        post(generation, [frame](ExcludingScreenCapture *capture) {
            emit capture->frameArrived(frame);
        });
    }

    // Step 1's answer: continue on the owner's thread. (Defined after Private.)
    void deliverContent(quint64 generation, id content, NSError *error, CGDirectDisplayID displayId, int framesPerSecond);

    void deliverError(quint64 generation, const QString &message, bool permission) {
        post(generation, [message, permission](ExcludingScreenCapture *capture) {
            capture->setRunning(false);
            emit capture->errorOccurred(message, permission);
        });
    }
};

API_AVAILABLE(macos(12.3))
@interface ScreenCamStreamOutput : NSObject <SCStreamOutput, SCStreamDelegate>
- (instancetype)initWithBridge:(std::shared_ptr<ExcludingScreenCapture::Bridge>)bridge generation:(quint64)generation;
@end

@implementation ScreenCamStreamOutput {
    std::shared_ptr<ExcludingScreenCapture::Bridge> _bridge;
    quint64 _generation;
}

- (instancetype)initWithBridge:(std::shared_ptr<ExcludingScreenCapture::Bridge>)bridge generation:(quint64)generation {
    self = [super init];
    if (self) {
        _bridge = std::move(bridge);
        _generation = generation;
    }
    return self;
}

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type {
    (void)stream;
    if (type != SCStreamOutputTypeScreen || !CMSampleBufferIsValid(sampleBuffer)) {
        return;
    }
    // Only "complete" samples carry a picture ("idle" ones mean nothing changed).
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
    if (attachments == nullptr || CFArrayGetCount(attachments) == 0) {
        return;
    }
    NSDictionary *info = (__bridge NSDictionary *)CFArrayGetValueAtIndex(attachments, 0);
    NSNumber *status = info[SCStreamFrameInfoStatus];
    if (status == nil || status.integerValue != SCFrameStatusComplete) {
        return;
    }
    CVPixelBufferRef pixels = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (pixels == nullptr) {
        return;
    }
    _bridge->deliverFrame(_generation, QVideoFrame(std::make_unique<PixelBufferVideoBuffer>(pixels)));
}

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error {
    (void)stream;
    _bridge->deliverError(_generation, QString::fromNSString(error.localizedDescription),
                          error.code == SCStreamErrorUserDeclined);
}

@end

struct ExcludingScreenCapture::Private {
    std::shared_ptr<Bridge> bridge = std::make_shared<Bridge>();
    id stream = nil;  // SCStream (typed `id`: the class only exists on macOS 12.3+)
    id output = nil;  // ScreenCamStreamOutput
    dispatch_queue_t queue = dispatch_queue_create("ScreenCam screen capture", DISPATCH_QUEUE_SERIAL);

    // Step 2 (main thread): build the filter and start. (`content`: SCShareableContent.)
    void startStream(ExcludingScreenCapture *q, id contentObject, NSError *error, CGDirectDisplayID displayId,
                     int framesPerSecond) {
        if (@available(macOS 12.3, *)) {
            startStreamWith(q, contentObject, error, displayId, framesPerSecond);
        }
    }

    void startStreamWith(ExcludingScreenCapture *q, SCShareableContent *content, NSError *error,
                         CGDirectDisplayID displayId, int framesPerSecond) API_AVAILABLE(macos(12.3));
};

void ExcludingScreenCapture::Bridge::deliverContent(quint64 generation, id content, NSError *error,
                                                    CGDirectDisplayID displayId, int framesPerSecond) {
    post(generation, [content, error, displayId, framesPerSecond](ExcludingScreenCapture *capture) {
        capture->d->startStream(capture, content, error, displayId, framesPerSecond);
    });
}

void ExcludingScreenCapture::Private::startStreamWith(ExcludingScreenCapture *q, SCShareableContent *content,
                                                      NSError *error, CGDirectDisplayID displayId,
                                                      int framesPerSecond) {
    if (error != nil || content == nil) {
        // Without Screen Recording permission, this is where macOS says no.
        q->setRunning(false);
        emit q->errorOccurred(error != nil ? QString::fromNSString(error.localizedDescription) : QString(),
                              error == nil || error.code == SCStreamErrorUserDeclined);
        return;
    }
    SCDisplay *display = nil;
    for (SCDisplay *candidate in content.displays) {
        if (candidate.displayID == displayId) {
            display = candidate;
        }
    }
    if (display == nil) {
        display = content.displays.firstObject;
    }
    if (display == nil) {
        q->setRunning(false);
        emit q->errorOccurred(QStringLiteral("No display to capture."), false);
        return;
    }

    // Every window of this app, including ones opened later.
    NSMutableArray<SCRunningApplication *> *excluded = [NSMutableArray array];
    const pid_t ownPid = getpid();
    for (SCRunningApplication *app in content.applications) {
        if (app.processID == ownPid) {
            [excluded addObject:app];
        }
    }
    SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:display
                                                excludingApplications:excluded
                                                     exceptingWindows:@[]];

    // Full pixel resolution: the region crop and the encoder scale it down as needed.
    SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
    size_t width = static_cast<size_t>(display.width) * 2;
    size_t height = static_cast<size_t>(display.height) * 2;
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display.displayID);
    if (mode != nullptr) {
        width = CGDisplayModeGetPixelWidth(mode);
        height = CGDisplayModeGetPixelHeight(mode);
        CGDisplayModeRelease(mode);
    }
    config.width = width;
    config.height = height;
    config.minimumFrameInterval = CMTimeMake(1, framesPerSecond);
    config.pixelFormat = kCVPixelFormatType_32BGRA;
    config.colorSpaceName = kCGColorSpaceSRGB;
    config.queueDepth = 8;  // the preview and the recorder each hold on to a frame
    // ScreenCam draws the pointer itself (core/Pointer.hpp).
    config.showsCursor = NO;

    ScreenCamStreamOutput *streamOutput = [[ScreenCamStreamOutput alloc] initWithBridge:bridge
                                                                             generation:q->m_generation];
    SCStream *newStream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:streamOutput];
    NSError *addError = nil;
    if (![newStream addStreamOutput:streamOutput type:SCStreamOutputTypeScreen sampleHandlerQueue:queue error:&addError]) {
        q->setRunning(false);
        emit q->errorOccurred(QString::fromNSString(addError.localizedDescription), false);
        return;
    }
    stream = newStream;
    output = streamOutput;

    std::shared_ptr<Bridge> sharedBridge = bridge;
    const quint64 generation = q->m_generation;
    [newStream startCaptureWithCompletionHandler:^(NSError *startError) {
        if (startError != nil) {
            sharedBridge->deliverError(generation, QString::fromNSString(startError.localizedDescription),
                                       startError.code == SCStreamErrorUserDeclined);
        }
    }];
}

ExcludingScreenCapture::ExcludingScreenCapture(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>()) {
    d->bridge->owner = this;
}

ExcludingScreenCapture::~ExcludingScreenCapture() {
    {
        std::lock_guard<std::mutex> lock(d->bridge->mutex);
        d->bridge->owner = nullptr;
    }
    m_generation++;
    if (@available(macOS 12.3, *)) {
        SCStream *stream = d->stream;
        id streamOutput = d->output;
        if (stream != nil) {
            [stream stopCaptureWithCompletionHandler:^(NSError *error) {
                (void)error;
                (void)stream;  // keeps the stream and its output alive until it has stopped
                (void)streamOutput;
            }];
        }
    }
}

bool ExcludingScreenCapture::isSupported() {
    if (@available(macOS 12.3, *)) {
        return true;
    }
    return false;
}

void ExcludingScreenCapture::setScreen(QScreen *screen) {
    if (screen == m_screen) {
        return;
    }
    m_screen = screen;
    if (m_wanted) {
        restart();
    }
}

void ExcludingScreenCapture::setFramesPerSecond(int framesPerSecond) {
    framesPerSecond = std::clamp(framesPerSecond, 1, 120);
    if (framesPerSecond == m_framesPerSecond) {
        return;
    }
    m_framesPerSecond = framesPerSecond;
    if (m_wanted) {
        restart();
    }
}

void ExcludingScreenCapture::setActive(bool active) {
    if (active == m_wanted) {
        return;
    }
    m_wanted = active;
    restart();
}

void ExcludingScreenCapture::restart() {
    stopStream();
    if (!m_wanted || m_screen == nullptr) {
        return;
    }
    if (@available(macOS 12.3, *)) {
        // Step 1: ask what's on screen; the answer comes on another thread.
        const quint64 generation = ++m_generation;
        const CGDirectDisplayID displayId = displayIdOf(m_screen);
        const int framesPerSecond = m_framesPerSecond;
        std::shared_ptr<Bridge> bridge = d->bridge;
        setRunning(true);
        [SCShareableContent
            getShareableContentExcludingDesktopWindows:NO
                                   onScreenWindowsOnly:NO
                                     completionHandler:^(SCShareableContent *content, NSError *error) {
                                         bridge->deliverContent(generation, content, error, displayId, framesPerSecond);
                                     }];
    }
}

void ExcludingScreenCapture::stopStream() {
    m_generation++;  // anything still in flight belongs to the old stream now
    if (@available(macOS 12.3, *)) {
        SCStream *stream = d->stream;
        id streamOutput = d->output;
        if (stream != nil) {
            [stream stopCaptureWithCompletionHandler:^(NSError *error) {
                (void)error;
                (void)stream;  // keeps the stream and its output alive until it has stopped
                (void)streamOutput;
            }];
        }
    }
    d->stream = nil;
    d->output = nil;
    setRunning(false);
}

void ExcludingScreenCapture::setRunning(bool running) {
    if (running != m_running) {
        m_running = running;
        emit activeChanged();
    }
}
