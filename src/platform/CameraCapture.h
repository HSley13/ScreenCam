#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

#include "core/Resolution.hpp"

NS_ASSUME_NONNULL_BEGIN

/// Receives raw camera frames. Called on a private background queue, never the main thread.
@protocol CameraFrameSink <NSObject>
/// `clock` is the capture session's clock; frames are timestamped on it.
- (void)cameraDidOutputVideoSampleBuffer:(CMSampleBufferRef)sampleBuffer fromClock:(nullable CMClockRef)clock;
@end

/// One long-lived AVCaptureSession for the selected camera. It runs as soon as a camera is
/// chosen (so the bubble shows a live preview), and frames are only *recorded* while a
/// RecordingSession is attached as `frameSink`.
@interface CameraCapture : NSObject

/// Live preview for the bubble. Mirrored, like FaceTime/Photo Booth.
@property (nonatomic, strong, readonly) AVCaptureVideoPreviewLayer *previewLayer;

/// Who gets frames right now (nil = nobody; preview still runs). Atomic + weak: written on the
/// main thread, read on the frame queue.
@property (atomic, weak, nullable) id<CameraFrameSink> frameSink;

/// uniqueID of the camera currently attached, nil when stopped.
@property (atomic, copy, readonly, nullable) NSString *currentDeviceID;

/// Switches cameras (nil stops the session). Work happens on a background queue because
/// -startRunning blocks; `completion` runs on the main queue.
- (void)switchToDeviceWithUniqueID:(nullable NSString *)uniqueID
                        completion:(void (^)(NSError *_Nullable error))completion;

/// Pixel size of the frames the camera currently produces ({0,0} when stopped).
- (screencam::core::PixelSize)activePixelSize;

@end

NS_ASSUME_NONNULL_END
