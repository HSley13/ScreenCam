#import "platform/MicCapture.h"

#import "platform/Errors.h"

@interface MicCapture () <AVCaptureAudioDataOutputSampleBufferDelegate>
@end

@implementation MicCapture {
    AVCaptureSession *_session;
    AVCaptureAudioDataOutput *_output;
    dispatch_queue_t _sessionQueue;
    dispatch_queue_t _sampleQueue;
    __weak id<MicCaptureSink> _sink;  // weak loads/stores are thread-safe
}

- (instancetype)initWithDeviceUniqueID:(NSString *)uniqueID sink:(id<MicCaptureSink>)sink error:(NSError **)error {
    if (!(self = [super init])) {
        return nil;
    }
    AVCaptureDevice *device = [AVCaptureDevice deviceWithUniqueID:uniqueID];
    if (device == nil) {
        if (error != nullptr) {
            *error = SCMakeError(@"The selected microphone is no longer connected.");
        }
        return nil;
    }
    AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:error];
    if (input == nil) {
        return nil;  // usually: microphone permission not granted
    }

    _session = [[AVCaptureSession alloc] init];
    _sessionQueue = dispatch_queue_create("com.hsley13.screencam.mic.session", DISPATCH_QUEUE_SERIAL);
    _sampleQueue = dispatch_queue_create("com.hsley13.screencam.mic.samples", DISPATCH_QUEUE_SERIAL);

    _output = [[AVCaptureAudioDataOutput alloc] init];
    // macOS-only feature: make every mic (44.1/48 kHz, mono/stereo, float/int) deliver the
    // same format, so the AAC encoder settings in MovieWriter always match.
    _output.audioSettings = @{
        AVFormatIDKey : @(kAudioFormatLinearPCM),
        AVSampleRateKey : @48000,
        AVNumberOfChannelsKey : @1,
        AVLinearPCMBitDepthKey : @16,
        AVLinearPCMIsFloatKey : @NO,
        AVLinearPCMIsBigEndianKey : @NO,
        AVLinearPCMIsNonInterleaved : @NO,
    };
    [_output setSampleBufferDelegate:self queue:_sampleQueue];

    [_session beginConfiguration];
    const BOOL ok = [_session canAddInput:input] && [_session canAddOutput:_output];
    if (ok) {
        [_session addInput:input];
        [_session addOutput:_output];
    }
    [_session commitConfiguration];
    if (!ok) {
        if (error != nullptr) {
            *error = SCMakeError(@"The selected microphone can't be used right now.");
        }
        return nil;
    }

    _sink = sink;
    return self;
}

- (void)startWithCompletion:(void (^)(void))completion {
    dispatch_async(_sessionQueue, ^{
        [self->_session startRunning];  // blocks until the device is running
        dispatch_async(dispatch_get_main_queue(), completion);
    });
}

- (void)stop {
    _sink = nil;  // stop forwarding immediately, even before the session has fully stopped
    dispatch_async(_sessionQueue, ^{
        [self->_session stopRunning];
    });
}

#pragma mark - AVCaptureAudioDataOutputSampleBufferDelegate

- (void)captureOutput:(AVCaptureOutput *)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection *)connection {
    id<MicCaptureSink> sink = _sink;
    [sink microphoneDidOutputAudioSampleBuffer:sampleBuffer fromClock:_session.synchronizationClock];
}

@end
