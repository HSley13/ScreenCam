#import "platform/ScreenCapture.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#import "platform/Errors.h"

namespace {

/// ScreenCaptureKit also delivers "idle" / "blank" buffers that carry no image (it only sends
/// new pixels when the screen changes). Only frames with status Complete may be encoded.
bool IsCompleteFrame(CMSampleBufferRef sampleBuffer) {
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
    if (attachments == nullptr || CFArrayGetCount(attachments) == 0) {
        return false;
    }
    NSDictionary *info = (__bridge NSDictionary *)CFArrayGetValueAtIndex(attachments, 0);
    NSNumber *status = info[SCStreamFrameInfoStatus];
    return status != nil && status.integerValue == SCFrameStatusComplete;
}

}  // namespace

@interface ScreenCapture () <SCStreamOutput, SCStreamDelegate>
@end

@implementation ScreenCapture {
    SCStream *_stream;              // only touched on the main queue
    dispatch_queue_t _sampleQueue;  // where frames arrive
}

- (instancetype)init {
    if ((self = [super init])) {
        _sampleQueue = dispatch_queue_create("com.hsley13.screencam.screen.frames", DISPATCH_QUEUE_SERIAL);
    }
    return self;
}

- (void)startWithDisplayID:(CGDirectDisplayID)displayID
                outputSize:(screencam::core::PixelSize)size
           framesPerSecond:(int)framesPerSecond
    excludingWindowNumbers:(NSArray<NSNumber *> *)windowNumbers
                completion:(void (^)(NSError *))completion {
    void (^finish)(NSError *) = ^(NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            completion(error);
        });
    };

    // Step 1: ask the system what can be captured (this is also what triggers the
    // Screen Recording permission check).
    [SCShareableContent
        getShareableContentExcludingDesktopWindows:NO
                               onScreenWindowsOnly:YES
                                 completionHandler:^(SCShareableContent *content, NSError *contentError) {
                                     if (content == nil) {
                                         finish(contentError != nil
                                                    ? contentError
                                                    : SCMakeError(@"Screen Recording permission is missing."));
                                         return;
                                     }
                                     dispatch_async(dispatch_get_main_queue(), ^{
                                         [self startStreamWithContent:content
                                                            displayID:displayID
                                                           outputSize:size
                                                      framesPerSecond:framesPerSecond
                                               excludingWindowNumbers:windowNumbers
                                                               finish:finish];
                                     });
                                 }];
}

/// Step 2 (main queue): build filter + configuration, create the stream and start it.
- (void)startStreamWithContent:(SCShareableContent *)content
                     displayID:(CGDirectDisplayID)displayID
                    outputSize:(screencam::core::PixelSize)size
               framesPerSecond:(int)framesPerSecond
        excludingWindowNumbers:(NSArray<NSNumber *> *)windowNumbers
                        finish:(void (^)(NSError *))finish {
    SCDisplay *display = nil;
    for (SCDisplay *candidate in content.displays) {
        if (candidate.displayID == displayID) {
            display = candidate;
            break;
        }
    }
    if (display == nil) {
        finish(SCMakeError(@"The selected screen is no longer available."));
        return;
    }

    // NSNumber's -isEqual: compares values, so NSInteger window numbers match CGWindowIDs.
    NSMutableArray<SCWindow *> *excluded = [NSMutableArray array];
    for (SCWindow *window in content.windows) {
        if ([windowNumbers containsObject:@(window.windowID)]) {
            [excluded addObject:window];
        }
    }
    SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:excluded];

    SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
    config.width = static_cast<size_t>(size.width);    // the stream scales the display to this
    config.height = static_cast<size_t>(size.height);
    config.minimumFrameInterval = CMTimeMake(1, framesPerSecond > 0 ? framesPerSecond : 30);
    config.pixelFormat = kCVPixelFormatType_32BGRA;
    config.showsCursor = YES;
    config.queueDepth = 6;  // frames the system may buffer before it starts dropping
    config.capturesAudio = NO;

    SCStream *stream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:self];
    NSError *addError = nil;
    if (![stream addStreamOutput:self type:SCStreamOutputTypeScreen sampleHandlerQueue:_sampleQueue error:&addError]) {
        finish(addError);
        return;
    }
    _stream = stream;

    [stream startCaptureWithCompletionHandler:^(NSError *startError) {
        if (startError != nil) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (self->_stream == stream) {
                    self->_stream = nil;
                }
            });
        }
        finish(startError);
    }];
}

- (void)stopWithCompletion:(void (^)(void))completion {
    SCStream *stream = _stream;
    _stream = nil;
    if (stream == nil) {
        dispatch_async(dispatch_get_main_queue(), completion);
        return;
    }
    [stream stopCaptureWithCompletionHandler:^(NSError *error) {
        dispatch_async(dispatch_get_main_queue(), completion);
    }];
}

#pragma mark - SCStreamOutput

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type {
    if (type != SCStreamOutputTypeScreen || !CMSampleBufferIsValid(sampleBuffer) || !IsCompleteFrame(sampleBuffer)) {
        return;
    }
    id<ScreenCaptureDelegate> delegate = self.delegate;
    [delegate screenCapture:self didOutputVideoSampleBuffer:sampleBuffer];
}

#pragma mark - SCStreamDelegate

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self->_stream != stream) {
            return;  // an old stream we already stopped on purpose
        }
        self->_stream = nil;
        [self.delegate screenCapture:self didStopWithError:error];
    });
}

@end
