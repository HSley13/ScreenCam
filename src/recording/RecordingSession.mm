#import "recording/RecordingSession.h"

#import "platform/Bridging.h"
#import "platform/CameraCapture.h"
#import "platform/ClockSync.h"
#import "platform/DeviceCatalog.h"
#import "platform/Errors.h"
#import "platform/MicCapture.h"
#import "platform/ScreenCapture.h"
#import "recording/MovieWriter.h"

#include "core/EncoderPlan.hpp"

namespace core = screencam::core;

namespace {

/// "2026-11-12 at 17.42.05" — sorts correctly and matches macOS screenshot naming.
NSString *TimestampString() {
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    formatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    formatter.dateFormat = @"yyyy-MM-dd 'at' HH.mm.ss";
    return [formatter stringFromDate:[NSDate date]];
}

}  // namespace

@implementation RecordingOptions

- (instancetype)init {
    if ((self = [super init])) {
        _displayID = CGMainDisplayID();
        _resolution = core::ResolutionPreset::FullHD1080;
        _cameraOrientation = core::CameraOrientation::Landscape;
        _framesPerSecond = 30;
        _excludedWindowNumbers = @[];
        NSURL *movies = [NSFileManager.defaultManager URLsForDirectory:NSMoviesDirectory inDomains:NSUserDomainMask].firstObject;
        if (movies == nil) {
            movies = [NSURL fileURLWithPath:[NSHomeDirectory() stringByAppendingPathComponent:@"Movies"] isDirectory:YES];
        }
        _outputDirectory = [movies URLByAppendingPathComponent:@"ScreenCam" isDirectory:YES];
    }
    return self;
}

@end

@interface RecordingSession () <ScreenCaptureDelegate, CameraFrameSink, MicCaptureSink>
@end

@implementation RecordingSession {
    RecordingOptions *_options;
    CameraCapture *_camera;  // shared with the bubble; we only borrow its frames
    ScreenCapture *_screen;
    MicCapture *_mic;
    // Writers are created before capture starts and never reassigned afterwards, so the
    // capture queues can read these ivars without locking.
    MovieWriter *_screenWriter;
    MovieWriter *_cameraWriter;
    core::PixelSize _screenSize;
    BOOL _stopping;
}

- (instancetype)initWithOptions:(RecordingOptions *)options camera:(CameraCapture *)camera {
    if ((self = [super init])) {
        _options = options;
        _camera = camera;
    }
    return self;
}

#pragma mark - Start

- (void)startWithCompletion:(void (^)(NSError *))completion {
    NSError *error = nil;
    if (![self prepareWritersWithError:&error]) {
        completion(error);
        return;
    }

    if (_options.microphoneID != nil) {
        _mic = [[MicCapture alloc] initWithDeviceUniqueID:_options.microphoneID sink:self error:&error];
        if (_mic == nil) {
            [self cancelWriters];
            completion(error);
            return;
        }
    }

    _screen = [[ScreenCapture alloc] init];
    _screen.delegate = self;

    // Order: mic first (it takes a moment to spin up), then camera frames, then the screen.
    // The movie starts at the first screen frame, so nothing important is lost.
    __weak RecordingSession *weakSelf = self;
    void (^startVideo)(void) = ^{
        [weakSelf startVideoWithCompletion:completion];
    };
    if (_mic != nil) {
        [_mic startWithCompletion:startVideo];
    } else {
        startVideo();
    }
}

/// Creates the output folder and one MovieWriter per file, sized by the portable core.
- (BOOL)prepareWritersWithError:(NSError **)error {
    if (![NSFileManager.defaultManager createDirectoryAtURL:_options.outputDirectory
                                withIntermediateDirectories:YES
                                                 attributes:nil
                                                      error:error]) {
        return NO;
    }

    NSString *stamp = TimestampString();
    const int fps = _options.framesPerSecond;
    const BOOL withAudio = (_options.microphoneID != nil);

    // Screen file: display pixels -> chosen preset (aspect kept, never upscaled, even dims).
    const core::PixelSize displayPixels = [DeviceCatalog pixelSizeForDisplay:_options.displayID];
    _screenSize = core::computeCaptureSize(displayPixels, _options.resolution);
    if (_screenSize.width <= 0 || _screenSize.height <= 0) {
        if (error != nullptr) {
            *error = SCMakeError(@"Couldn't read the selected screen's resolution.");
        }
        return NO;
    }
    NSURL *screenURL = [_options.outputDirectory
        URLByAppendingPathComponent:[NSString stringWithFormat:@"ScreenCam %@ - Screen.mov", stamp]];
    _screenWriter = [[MovieWriter alloc] initWithURL:screenURL
                                                size:_screenSize
                                                plan:core::planEncoder(_screenSize, fps)
                                        includeAudio:withAudio
                                               error:error];
    if (_screenWriter == nil) {
        return NO;
    }

    // Camera file: camera pixels -> largest centred 16:9 or 9:16 rectangle.
    if (_options.recordCamera && _camera != nil) {
        const core::PixelSize cameraSize = core::cameraRecordingSize([_camera activePixelSize], _options.cameraOrientation);
        if (cameraSize.width > 0 && cameraSize.height > 0) {
            NSString *name = [NSString stringWithFormat:@"ScreenCam %@ - Camera (%@).mov", stamp,
                                                        SCString(core::orientationLabel(_options.cameraOrientation))];
            _cameraWriter = [[MovieWriter alloc] initWithURL:[_options.outputDirectory URLByAppendingPathComponent:name]
                                                        size:cameraSize
                                                        plan:core::planEncoder(cameraSize, fps)
                                                includeAudio:withAudio
                                                       error:error];
            if (_cameraWriter == nil) {
                [_screenWriter cancel];
                return NO;
            }
        }
    }
    return YES;
}

- (void)startVideoWithCompletion:(void (^)(NSError *))completion {
    if (_cameraWriter != nil) {
        _camera.frameSink = self;
    }
    __weak RecordingSession *weakSelf = self;
    [_screen startWithDisplayID:_options.displayID
                     outputSize:_screenSize
                framesPerSecond:_options.framesPerSecond
         excludingWindowNumbers:_options.excludedWindowNumbers
                     completion:^(NSError *error) {
                         if (error != nil) {
                             [weakSelf abortAfterFailedStart];
                         }
                         completion(error);
                     }];
}

- (void)abortAfterFailedStart {
    [self detachCamera];
    [_mic stop];
    [self cancelWriters];
}

- (void)cancelWriters {
    [_screenWriter cancel];
    [_cameraWriter cancel];
}

- (void)detachCamera {
    if (_camera.frameSink == self) {
        _camera.frameSink = nil;
    }
}

#pragma mark - Stop

- (void)stopWithCompletion:(void (^)(NSArray<NSURL *> *, NSError *))completion {
    if (_stopping) {
        return;
    }
    _stopping = YES;

    // The moment Stop was pressed becomes the end of both movies.
    const CMTime endTime = CMClockGetTime(CMClockGetHostTimeClock());

    [self detachCamera];
    [_mic stop];

    NSMutableArray<MovieWriter *> *writers = [NSMutableArray array];
    if (_screenWriter != nil) {
        [writers addObject:_screenWriter];
    }
    if (_cameraWriter != nil) {
        [writers addObject:_cameraWriter];
    }

    [_screen stopWithCompletion:^{
        // Finish all files in parallel, then report once every one is done.
        dispatch_group_t group = dispatch_group_create();
        for (MovieWriter *writer in writers) {
            dispatch_group_enter(group);
            [writer finishAtSourceTime:endTime
                            completion:^{
                                dispatch_group_leave(group);
                            }];
        }
        dispatch_group_notify(group, dispatch_get_main_queue(), ^{
            NSMutableArray<NSURL *> *files = [NSMutableArray array];
            NSError *firstError = nil;
            for (MovieWriter *writer in writers) {
                if (writer.finishError == nil) {
                    [files addObject:writer.url];
                } else if (firstError == nil) {
                    firstError = writer.finishError;
                }
            }
            completion(files, firstError);
        });
    }];
}

#pragma mark - ScreenCaptureDelegate

- (void)screenCapture:(ScreenCapture *)capture didOutputVideoSampleBuffer:(CMSampleBufferRef)sampleBuffer {
    // ScreenCaptureKit already stamps frames on the host clock: no retiming needed.
    [_screenWriter appendVideoSampleBuffer:sampleBuffer];
}

- (void)screenCapture:(ScreenCapture *)capture didStopWithError:(NSError *)error {
    if (_stopping) {
        return;
    }
    if (self.onUnexpectedStop != nil) {
        self.onUnexpectedStop(error);
    }
}

#pragma mark - CameraFrameSink

- (void)cameraDidOutputVideoSampleBuffer:(CMSampleBufferRef)sampleBuffer fromClock:(CMClockRef)clock {
    CMSampleBufferRef retimed = SCCopySampleBufferOnHostClock(sampleBuffer, clock);
    if (retimed == nullptr) {
        return;
    }
    [_cameraWriter appendVideoSampleBuffer:retimed];
    CFRelease(retimed);
}

#pragma mark - MicCaptureSink

- (void)microphoneDidOutputAudioSampleBuffer:(CMSampleBufferRef)sampleBuffer fromClock:(CMClockRef)clock {
    CMSampleBufferRef retimed = SCCopySampleBufferOnHostClock(sampleBuffer, clock);
    if (retimed == nullptr) {
        return;
    }
    // One microphone, two movies: the same audio goes into both files.
    [_screenWriter appendAudioSampleBuffer:retimed];
    [_cameraWriter appendAudioSampleBuffer:retimed];
    CFRelease(retimed);
}

@end
