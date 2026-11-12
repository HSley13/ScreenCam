#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MicCaptureSink <NSObject>
/// 48 kHz mono 16-bit PCM. Called on a background queue.
- (void)microphoneDidOutputAudioSampleBuffer:(CMSampleBufferRef)sampleBuffer fromClock:(nullable CMClockRef)clock;
@end

/// Captures one microphone. Created per recording (unlike the camera), so the orange
/// "mic in use" indicator only shows while you are actually recording.
@interface MicCapture : NSObject

- (nullable instancetype)initWithDeviceUniqueID:(NSString *)uniqueID
                                           sink:(id<MicCaptureSink>)sink
                                          error:(NSError **)error;

/// Starts the session in the background; `completion` runs on the main queue once audio flows.
- (void)startWithCompletion:(void (^)(void))completion;
- (void)stop;

@end

NS_ASSUME_NONNULL_END
