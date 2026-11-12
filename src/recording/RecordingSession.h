#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#include "core/CameraLayout.hpp"
#include "core/Resolution.hpp"

@class CameraCapture;

NS_ASSUME_NONNULL_BEGIN

/// Everything one recording needs to know, captured from the panel when you press Record.
@interface RecordingOptions : NSObject
@property (nonatomic) CGDirectDisplayID displayID;
@property (nonatomic) screencam::core::ResolutionPreset resolution;
@property (nonatomic) screencam::core::CameraOrientation cameraOrientation;
@property (nonatomic) BOOL recordCamera;                         // also write the separate camera file
@property (nonatomic, copy, nullable) NSString *microphoneID;    // nil = no audio
@property (nonatomic) int framesPerSecond;                       // default 30
@property (nonatomic, copy) NSArray<NSNumber *> *excludedWindowNumbers;
@property (nonatomic, copy) NSURL *outputDirectory;              // default ~/Movies/ScreenCam
@end

/// One recording from Start to Stop. Produces up to two files with the same timestamp:
///   "ScreenCam <date> - Screen.mov"            screen + bubble + mic audio
///   "ScreenCam <date> - Camera (Portrait).mov" just you, framed 9:16 or 16:9, + mic audio
///
/// Data flow:  ScreenCapture ──frames──┐
///             CameraCapture ──frames──┼──> RecordingSession ──> MovieWriter(s)
///             MicCapture    ──audio───┘     (retimes to host clock, fans audio out to both)
@interface RecordingSession : NSObject

- (instancetype)initWithOptions:(RecordingOptions *)options camera:(nullable CameraCapture *)camera;

/// `completion` runs on the main queue; a non-nil error means nothing is recording.
- (void)startWithCompletion:(void (^)(NSError *_Nullable error))completion;

/// Stops capture and finalizes the files. Calling it twice is a no-op for the second call.
/// `files` lists the movies that were saved (main queue).
- (void)stopWithCompletion:(void (^)(NSArray<NSURL *> *files, NSError *_Nullable error))completion;

/// Called on the main queue if the system ends the screen stream on its own.
@property (nonatomic, copy, nullable) void (^onUnexpectedStop)(NSError *error);

@end

NS_ASSUME_NONNULL_END
