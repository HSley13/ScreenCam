#import "platform/DeviceCatalog.h"

#import <AVFoundation/AVFoundation.h>

namespace {

/// Converts AVCaptureDevices to SourceInfo, dropping duplicates. Discovery sessions can report
/// the same device under two device types (e.g. "external" and "microphone").
NSArray<SourceInfo *> *SourcesFromDevices(NSArray<AVCaptureDevice *> *devices) {
    NSMutableArray<SourceInfo *> *sources = [NSMutableArray array];
    NSMutableSet<NSString *> *seen = [NSMutableSet set];
    for (AVCaptureDevice *device in devices) {
        if ([seen containsObject:device.uniqueID]) {
            continue;
        }
        [seen addObject:device.uniqueID];
        [sources addObject:[[SourceInfo alloc] initWithUniqueID:device.uniqueID name:device.localizedName]];
    }
    return sources;
}

}  // namespace

@implementation SourceInfo
- (instancetype)initWithUniqueID:(NSString *)uniqueID name:(NSString *)name {
    if ((self = [super init])) {
        _uniqueID = [uniqueID copy];
        _name = [name copy];
    }
    return self;
}
@end

@implementation DisplayInfo
- (instancetype)initWithDisplayID:(CGDirectDisplayID)displayID
                             name:(NSString *)name
                        pixelSize:(screencam::core::PixelSize)pixelSize {
    if ((self = [super init])) {
        _displayID = displayID;
        _name = [name copy];
        _pixelSize = pixelSize;
    }
    return self;
}
@end

@implementation DeviceCatalog

+ (NSArray<SourceInfo *> *)cameras {
    // Built-in "FaceTime HD" camera + USB webcams + iPhone Continuity Camera.
    NSMutableArray<AVCaptureDeviceType> *types = [NSMutableArray arrayWithObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
    if (@available(macOS 14.0, *)) {
        [types addObject:AVCaptureDeviceTypeExternal];
        [types addObject:AVCaptureDeviceTypeContinuityCamera];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [types addObject:AVCaptureDeviceTypeExternalUnknown];
#pragma clang diagnostic pop
    }
    AVCaptureDeviceDiscoverySession *discovery =
        [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:types
                                                               mediaType:AVMediaTypeVideo
                                                                position:AVCaptureDevicePositionUnspecified];
    return SourcesFromDevices(discovery.devices);
}

+ (NSArray<SourceInfo *> *)microphones {
    if (@available(macOS 14.0, *)) {
        AVCaptureDeviceDiscoverySession *discovery = [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeMicrophone, AVCaptureDeviceTypeExternal]
                                  mediaType:AVMediaTypeAudio
                                   position:AVCaptureDevicePositionUnspecified];
        return SourcesFromDevices(discovery.devices);
    }
    // macOS 13: the old API still lists every audio input (built-in, USB, AirPods, ...).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return SourcesFromDevices([AVCaptureDevice devicesWithMediaType:AVMediaTypeAudio]);
#pragma clang diagnostic pop
}

+ (NSString *)defaultCameraID {
    return [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo].uniqueID;
}

+ (NSString *)defaultMicrophoneID {
    return [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeAudio].uniqueID;
}

+ (NSArray<DisplayInfo *> *)displays {
    NSMutableArray<DisplayInfo *> *result = [NSMutableArray array];
    for (NSScreen *screen in NSScreen.screens) {
        const CGDirectDisplayID displayID = [self displayIDForScreen:screen];
        if (displayID == kCGNullDirectDisplay) {
            continue;
        }
        [result addObject:[[DisplayInfo alloc] initWithDisplayID:displayID
                                                            name:screen.localizedName
                                                       pixelSize:[self pixelSizeForDisplay:displayID]]];
    }
    return result;
}

+ (screencam::core::PixelSize)pixelSizeForDisplay:(CGDirectDisplayID)displayID {
    // NSScreen/SCDisplay sizes are in *points*; the display mode knows the real pixel count.
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayID);
    if (mode == nullptr) {
        return {};
    }
    const screencam::core::PixelSize size{static_cast<int>(CGDisplayModeGetPixelWidth(mode)),
                                          static_cast<int>(CGDisplayModeGetPixelHeight(mode))};
    CGDisplayModeRelease(mode);
    return size;
}

+ (NSScreen *)screenForDisplayID:(CGDirectDisplayID)displayID {
    for (NSScreen *screen in NSScreen.screens) {
        if ([self displayIDForScreen:screen] == displayID) {
            return screen;
        }
    }
    return nil;
}

+ (CGDirectDisplayID)displayIDForScreen:(NSScreen *)screen {
    NSNumber *number = screen.deviceDescription[@"NSScreenNumber"];
    return (number != nil) ? static_cast<CGDirectDisplayID>(number.unsignedIntValue) : kCGNullDirectDisplay;
}

@end
