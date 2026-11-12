#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

#include "core/Resolution.hpp"

NS_ASSUME_NONNULL_BEGIN

@class ScreenCapture;

@protocol ScreenCaptureDelegate <NSObject>
/// A complete screen frame (BGRA, host-clock timestamps). Called on a background queue.
- (void)screenCapture:(ScreenCapture *)capture didOutputVideoSampleBuffer:(CMSampleBufferRef)sampleBuffer;
/// The system stopped the stream (display unplugged, permission revoked, ...). Main queue.
- (void)screenCapture:(ScreenCapture *)capture didStopWithError:(NSError *)error;
@end

/// Thin wrapper around a ScreenCaptureKit SCStream that records one display.
@interface ScreenCapture : NSObject

@property (nonatomic, weak, nullable) id<ScreenCaptureDelegate> delegate;

/// Starts streaming `displayID` scaled to `size`. Windows whose window numbers are listed are
/// left out of the picture (we exclude the control panel; the camera bubble stays in).
/// `completion` runs on the main queue.
- (void)startWithDisplayID:(CGDirectDisplayID)displayID
                outputSize:(screencam::core::PixelSize)size
           framesPerSecond:(int)framesPerSecond
    excludingWindowNumbers:(NSArray<NSNumber *> *)windowNumbers
                completion:(void (^)(NSError *_Nullable error))completion;

/// Stops the stream; `completion` runs on the main queue (immediately if not running).
- (void)stopWithCompletion:(void (^)(void))completion;

@end

NS_ASSUME_NONNULL_END
