#import "recording/MovieWriter.h"

#import "platform/Errors.h"

namespace core = screencam::core;

namespace {

/// H.264/HEVC settings. The scaling mode is what makes "different input size" safe:
/// ResizeAspectFill = keep aspect ratio, fill the output, crop what sticks out.
NSDictionary *VideoSettings(core::PixelSize size, const core::EncoderPlan &plan) {
    NSMutableDictionary *compression = [@{
        AVVideoAverageBitRateKey : @(plan.averageBitRate),
        AVVideoMaxKeyFrameIntervalKey : @(plan.maxKeyFrameInterval),
        AVVideoExpectedSourceFrameRateKey : @(plan.framesPerSecond),
        AVVideoAllowFrameReorderingKey : @NO,  // no B-frames: lower latency for live input
    } mutableCopy];

    NSString *codec = AVVideoCodecTypeH264;
    if (plan.codec == core::VideoCodec::HEVC) {
        codec = AVVideoCodecTypeHEVC;
    } else {
        compression[AVVideoProfileLevelKey] = AVVideoProfileLevelH264HighAutoLevel;
    }

    return @{
        AVVideoCodecKey : codec,
        AVVideoWidthKey : @(size.width),
        AVVideoHeightKey : @(size.height),
        AVVideoScalingModeKey : AVVideoScalingModeResizeAspectFill,
        AVVideoCompressionPropertiesKey : compression,
    };
}

/// Mono AAC at 48 kHz — matches what MicCapture delivers (it is forced to 48 kHz mono PCM).
NSDictionary *AudioSettings() {
    AudioChannelLayout layout{};
    layout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
    return @{
        AVFormatIDKey : @(kAudioFormatMPEG4AAC),
        AVSampleRateKey : @48000,
        AVNumberOfChannelsKey : @1,
        AVEncoderBitRateKey : @96000,
        AVChannelLayoutKey : [NSData dataWithBytes:&layout length:sizeof(layout)],
    };
}

}  // namespace

@implementation MovieWriter {
    AVAssetWriter *_writer;
    AVAssetWriterInput *_videoInput;
    AVAssetWriterInput *_audioInput;  // nil when recording without a microphone
    BOOL _sessionStarted;             // startSessionAtSourceTime: has been called
    BOOL _finishing;                  // no more appends accepted
    CMTime _startTime;
    CMTime _lastVideoTime;
    NSUInteger _droppedVideoFrames;
}

- (instancetype)initWithURL:(NSURL *)url
                       size:(core::PixelSize)size
                       plan:(core::EncoderPlan)plan
               includeAudio:(BOOL)includeAudio
                      error:(NSError **)error {
    if (!(self = [super init])) {
        return nil;
    }
    _url = [url copy];
    _startTime = kCMTimeInvalid;
    _lastVideoTime = kCMTimeInvalid;

    // AVAssetWriter refuses to overwrite an existing file.
    [NSFileManager.defaultManager removeItemAtURL:url error:nil];

    _writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeQuickTimeMovie error:error];
    if (_writer == nil) {
        return nil;
    }

    NSDictionary *videoSettings = VideoSettings(size, plan);
    if (![_writer canApplyOutputSettings:videoSettings forMediaType:AVMediaTypeVideo]) {
        if (error != nullptr) {
            *error = SCMakeError([NSString stringWithFormat:@"This Mac can't encode %d × %d video.", size.width, size.height]);
        }
        return nil;
    }
    _videoInput = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo outputSettings:videoSettings];
    _videoInput.expectsMediaDataInRealTime = YES;  // tune for live sources: never block, drop instead
    if (![_writer canAddInput:_videoInput]) {
        if (error != nullptr) {
            *error = SCMakeError(@"Couldn't add a video track to the movie.");
        }
        return nil;
    }
    [_writer addInput:_videoInput];

    if (includeAudio) {
        _audioInput = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeAudio outputSettings:AudioSettings()];
        _audioInput.expectsMediaDataInRealTime = YES;
        if (![_writer canAddInput:_audioInput]) {
            if (error != nullptr) {
                *error = SCMakeError(@"Couldn't add an audio track to the movie.");
            }
            return nil;
        }
        [_writer addInput:_audioInput];
    }

    if (![_writer startWriting]) {
        if (error != nullptr) {
            *error = (_writer.error != nil) ? _writer.error : SCMakeError(@"Couldn't start writing the movie file.");
        }
        return nil;
    }
    return self;
}

- (void)appendVideoSampleBuffer:(CMSampleBufferRef)sampleBuffer {
    @synchronized(self) {
        if (_finishing || _writer.status != AVAssetWriterStatusWriting) {
            return;
        }
        const CMTime pts = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
        if (!CMTIME_IS_VALID(pts)) {
            return;
        }
        if (!_sessionStarted) {
            // The first video frame defines t = 0 of the movie.
            [_writer startSessionAtSourceTime:pts];
            _sessionStarted = YES;
            _startTime = pts;
        } else if (CMTIME_COMPARE_INLINE(pts, <=, _lastVideoTime)) {
            return;  // the encoder requires strictly increasing timestamps
        }
        if (!_videoInput.isReadyForMoreMediaData) {
            _droppedVideoFrames++;  // encoder is busy: dropping one frame beats stalling capture
            return;
        }
        if ([_videoInput appendSampleBuffer:sampleBuffer]) {
            _lastVideoTime = pts;
        }
    }
}

- (void)appendAudioSampleBuffer:(CMSampleBufferRef)sampleBuffer {
    @synchronized(self) {
        if (_audioInput == nil || _finishing || !_sessionStarted || _writer.status != AVAssetWriterStatusWriting) {
            return;
        }
        const CMTime pts = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
        if (!CMTIME_IS_VALID(pts) || CMTIME_COMPARE_INLINE(pts, <, _startTime)) {
            return;  // audio from before the first video frame
        }
        if (_audioInput.isReadyForMoreMediaData) {
            [_audioInput appendSampleBuffer:sampleBuffer];
        }
    }
}

- (void)finishAtSourceTime:(CMTime)endTime completion:(void (^)(void))completion {
    BOOL started = NO;
    @synchronized(self) {
        if (_finishing) {
            completion();
            return;
        }
        // From here on every append returns early; the lock guarantees no append is mid-flight.
        _finishing = YES;
        started = _sessionStarted;
    }

    if (!started || _writer.status != AVAssetWriterStatusWriting) {
        NSError *writerError = _writer.error;
        [_writer cancelWriting];
        [NSFileManager.defaultManager removeItemAtURL:_url error:nil];
        _finishError = (writerError != nil) ? writerError
                                            : SCMakeError(@"No video frames were captured, so nothing was saved.");
        completion();
        return;
    }

    // Ending the session at the stop time (not at the last frame) matters for screen
    // recordings: ScreenCaptureKit only sends frames when something changes, so a static
    // screen would otherwise cut the video short while the audio keeps going.
    CMTime sessionEnd = endTime;
    if (!CMTIME_IS_VALID(sessionEnd) || CMTIME_COMPARE_INLINE(sessionEnd, <=, _startTime)) {
        sessionEnd = _lastVideoTime;
    }
    [_writer endSessionAtSourceTime:sessionEnd];

    if (_droppedVideoFrames > 0) {
        NSLog(@"ScreenCam: %lu video frame(s) dropped while writing %@", static_cast<unsigned long>(_droppedVideoFrames),
              _url.lastPathComponent);
    }

    [_writer finishWritingWithCompletionHandler:^{
        if (self->_writer.status != AVAssetWriterStatusCompleted) {
            self->_finishError = (self->_writer.error != nil) ? self->_writer.error
                                                              : SCMakeError(@"Saving the movie failed.");
        }
        completion();
    }];
}

- (void)cancel {
    @synchronized(self) {
        _finishing = YES;
    }
    if (_writer.status == AVAssetWriterStatusWriting) {
        [_writer cancelWriting];
    }
    [NSFileManager.defaultManager removeItemAtURL:_url error:nil];
}

@end
