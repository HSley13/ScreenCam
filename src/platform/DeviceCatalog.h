#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include "core/Resolution.hpp"

NS_ASSUME_NONNULL_BEGIN

/// A camera or microphone, reduced to what the UI needs: a stable id and a display name.
@interface SourceInfo : NSObject
@property (nonatomic, copy, readonly) NSString *uniqueID;  // AVCaptureDevice.uniqueID
@property (nonatomic, copy, readonly) NSString *name;      // e.g. "FaceTime HD Camera"
- (instancetype)initWithUniqueID:(NSString *)uniqueID name:(NSString *)name;
@end

/// A physical display that can be recorded.
@interface DisplayInfo : NSObject
@property (nonatomic, readonly) CGDirectDisplayID displayID;
@property (nonatomic, copy, readonly) NSString *name;
@property (nonatomic, readonly) screencam::core::PixelSize pixelSize;  // real (Retina) pixels
- (instancetype)initWithDisplayID:(CGDirectDisplayID)displayID
                             name:(NSString *)name
                        pixelSize:(screencam::core::PixelSize)pixelSize;
@end

/// Read-only queries about the hardware currently attached to the Mac.
/// Every call re-asks the system, so results reflect hot-plugged devices.
@interface DeviceCatalog : NSObject
@property (class, nonatomic, readonly) NSArray<SourceInfo *> *cameras;
@property (class, nonatomic, readonly) NSArray<SourceInfo *> *microphones;
@property (class, nonatomic, readonly) NSArray<DisplayInfo *> *displays;
@property (class, nonatomic, readonly, nullable) NSString *defaultCameraID;
@property (class, nonatomic, readonly, nullable) NSString *defaultMicrophoneID;

/// Pixel size of the display's current mode ({0,0} if the display is gone).
+ (screencam::core::PixelSize)pixelSizeForDisplay:(CGDirectDisplayID)displayID;
/// The NSScreen that shows `displayID`, used to place the camera bubble on it.
+ (nullable NSScreen *)screenForDisplayID:(CGDirectDisplayID)displayID;
/// The CGDirectDisplayID behind an NSScreen (kCGNullDirectDisplay if unknown).
+ (CGDirectDisplayID)displayIDForScreen:(NSScreen *)screen;
@end

NS_ASSUME_NONNULL_END
