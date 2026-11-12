#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/// A borderless, always-on-top, circular window that shows the camera preview.
///
/// Why a real window instead of drawing the circle into the video ourselves? Because the
/// screen capture records whatever is on screen: wherever you drag this window is exactly
/// where it appears in the recording — you see the final framing live, and there is no
/// per-frame compositing code to get wrong.
@interface CameraBubbleWindow : NSPanel

- (instancetype)initWithPreviewLayer:(AVCaptureVideoPreviewLayer *)previewLayer diameter:(CGFloat)diameter;

/// Moves the bubble to the bottom-left corner of `screen` — unless it is already on that
/// screen, in which case the position you dragged it to is kept.
- (void)placeOnScreen:(nullable NSScreen *)screen;

/// Resizes the circle around its current centre (also reachable via right-click).
- (void)setDiameter:(CGFloat)diameter;

@end

NS_ASSUME_NONNULL_END
