#import "platform/CameraCapture.h"

#import "platform/Errors.h"

@interface CameraCapture () <AVCaptureVideoDataOutputSampleBufferDelegate>
@property (atomic, copy, readwrite, nullable) NSString *currentDeviceID;
@property (atomic, strong, nullable) AVCaptureDevice *activeDevice;
@end

@implementation CameraCapture {
    AVCaptureSession *_session;
    AVCaptureDeviceInput *_input;       // only touched on _sessionQueue
    AVCaptureVideoDataOutput *_output;  // raw frames for the camera recording
    dispatch_queue_t _sessionQueue;     // configuration + start/stop (these calls block)
    dispatch_queue_t _frameQueue;       // where sample buffers are delivered
}

- (instancetype)init {
    if ((self = [super init])) {
        _session = [[AVCaptureSession alloc] init];
        _sessionQueue = dispatch_queue_create("com.hsley13.screencam.camera.session", DISPATCH_QUEUE_SERIAL);
        _frameQueue = dispatch_queue_create("com.hsley13.screencam.camera.frames", DISPATCH_QUEUE_SERIAL);

        _previewLayer = [AVCaptureVideoPreviewLayer layerWithSession:_session];
        _previewLayer.videoGravity = AVLayerVideoGravityResizeAspectFill;  // fill the circle, crop edges

        _output = [[AVCaptureVideoDataOutput alloc] init];
        _output.alwaysDiscardsLateVideoFrames = YES;  // never let a slow encoder back up the camera
        _output.videoSettings = @{
            (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
        };
        [_output setSampleBufferDelegate:self queue:_frameQueue];
        if ([_session canAddOutput:_output]) {
            [_session addOutput:_output];
        }
    }
    return self;
}

- (void)switchToDeviceWithUniqueID:(NSString *)uniqueID completion:(void (^)(NSError *))completion {
    dispatch_async(_sessionQueue, ^{
        NSError *error = [self reconfigureForDeviceID:uniqueID];
        dispatch_async(dispatch_get_main_queue(), ^{
            completion(error);
        });
    });
}

/// Runs on _sessionQueue. Swaps the input inside begin/commitConfiguration so the change is
/// atomic, then starts or stops the session. Returns an error if the camera couldn't attach.
- (nullable NSError *)reconfigureForDeviceID:(nullable NSString *)uniqueID {
    NSError *error = nil;

    [_session beginConfiguration];
    if (_input != nil) {
        [_session removeInput:_input];
        _input = nil;
    }

    AVCaptureDevice *device = (uniqueID != nil) ? [AVCaptureDevice deviceWithUniqueID:uniqueID] : nil;
    if (uniqueID != nil && device == nil) {
        error = SCMakeError(@"The selected camera is no longer connected.");
    }
    if (device != nil) {
        AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
        if (input != nil && [_session canAddInput:input]) {
            [_session addInput:input];
            _input = input;
            [self applyBestPreset];
        } else if (error == nil) {
            error = SCMakeError(@"The selected camera can't be used right now (is another app using it?).");
        }
    }
    [_session commitConfiguration];

    [self applyMirroring];
    self.activeDevice = _input.device;
    self.currentDeviceID = _input.device.uniqueID;

    if (_input != nil && !_session.running) {
        [_session startRunning];
    } else if (_input == nil && _session.running) {
        [_session stopRunning];
    }
    return error;
}

/// Asks for 1080p, then 720p, then "whatever is best". Must run inside begin/commit.
- (void)applyBestPreset {
    for (AVCaptureSessionPreset preset in @[AVCaptureSessionPreset1920x1080, AVCaptureSessionPreset1280x720,
                                            AVCaptureSessionPresetHigh]) {
        if ([_session canSetSessionPreset:preset]) {
            _session.sessionPreset = preset;
            return;
        }
    }
}

/// Mirror only the *preview* (people expect a mirror image of themselves). The recorded
/// frames stay un-mirrored so text on a shirt or whiteboard reads correctly.
- (void)applyMirroring {
    AVCaptureConnection *connection = _previewLayer.connection;
    if (connection != nil && connection.isVideoMirroringSupported) {
        connection.automaticallyAdjustsVideoMirroring = NO;
        connection.videoMirrored = YES;
    }
}

- (screencam::core::PixelSize)activePixelSize {
    AVCaptureDevice *device = self.activeDevice;
    if (device == nil) {
        return {};
    }
    const CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(device.activeFormat.formatDescription);
    return {dims.width, dims.height};
}

#pragma mark - AVCaptureVideoDataOutputSampleBufferDelegate

- (void)captureOutput:(AVCaptureOutput *)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection *)connection {
    id<CameraFrameSink> sink = self.frameSink;  // take a strong reference once
    if (sink != nil) {
        [sink cameraDidOutputVideoSampleBuffer:sampleBuffer fromClock:_session.synchronizationClock];
    }
}

@end
