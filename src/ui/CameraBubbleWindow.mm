#import "ui/CameraBubbleWindow.h"

#import <QuartzCore/QuartzCore.h>

#import "platform/DeviceCatalog.h"

namespace {
constexpr CGFloat kScreenMargin = 32.0;
constexpr CGFloat kBorderWidth = 3.0;
}  // namespace

/// Content view of the bubble. It handles dragging: on mouse-down we hand the event to the
/// window server (-performWindowDragWithEvent:), which moves the window smoothly for us.
@interface BubbleView : NSView
@end

@implementation BubbleView

/// Lets the very first click start a drag even when ScreenCam isn't the active app.
- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return YES;
}

- (void)mouseDown:(NSEvent *)event {
    [self.window performWindowDragWithEvent:event];
}

- (void)resetCursorRects {
    [self addCursorRect:self.bounds cursor:NSCursor.openHandCursor];
}

@end

@implementation CameraBubbleWindow {
    CALayer *_circleLayer;  // clips everything to a circle and draws the white ring
    AVCaptureVideoPreviewLayer *_previewLayer;
}

- (instancetype)initWithPreviewLayer:(AVCaptureVideoPreviewLayer *)previewLayer diameter:(CGFloat)diameter {
    const NSRect frame = NSMakeRect(0, 0, diameter, diameter);
    // NonactivatingPanel: clicking/dragging the bubble does NOT bring ScreenCam to the front,
    // so the app you are presenting keeps focus.
    self = [super initWithContentRect:frame
                            styleMask:(NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel)
                              backing:NSBackingStoreBuffered
                                defer:NO];
    if (self == nil) {
        return nil;
    }

    // Transparent window: only the circular layer below is visible.
    self.opaque = NO;
    self.backgroundColor = NSColor.clearColor;
    self.hasShadow = NO;  // the window shadow would be computed before the circle is drawn (square)

    // Stay above normal windows of every app, on every Space, even next to full-screen apps.
    self.floatingPanel = YES;
    self.level = NSFloatingWindowLevel;
    self.hidesOnDeactivate = NO;  // NSPanel hides when its app is inactive by default — not wanted here
    self.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                              NSWindowCollectionBehaviorFullScreenAuxiliary |
                              NSWindowCollectionBehaviorIgnoresCycle;
    self.releasedWhenClosed = NO;

    _previewLayer = previewLayer;
    _circleLayer = [CALayer layer];
    _circleLayer.backgroundColor = NSColor.blackColor.CGColor;  // shown until the first frame
    _circleLayer.masksToBounds = YES;                            // cornerRadius + mask = circle
    _circleLayer.borderWidth = kBorderWidth;                     // drawn above sublayers
    _circleLayer.borderColor = [NSColor colorWithWhite:1.0 alpha:0.9].CGColor;
    [_circleLayer addSublayer:previewLayer];

    BubbleView *view = [[BubbleView alloc] initWithFrame:frame];
    // Setting `layer` *before* `wantsLayer` makes this a layer-hosting view: we own the layer
    // tree completely and AppKit won't reset its properties.
    view.layer = _circleLayer;
    view.wantsLayer = YES;
    view.menu = [self buildSizeMenu];  // right-click menu
    self.contentView = view;

    [self layoutCircleForDiameter:diameter];
    return self;
}

- (NSMenu *)buildSizeMenu {
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Bubble Size"];
    NSArray<NSString *> *titles = @[@"Small", @"Medium", @"Large"];
    const CGFloat diameters[] = {120.0, 180.0, 260.0};
    for (NSUInteger i = 0; i < titles.count; i++) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:titles[i]
                                                      action:@selector(sizeMenuItemChosen:)
                                               keyEquivalent:@""];
        item.target = self;
        item.tag = static_cast<NSInteger>(diameters[i]);
        [menu addItem:item];
    }
    return menu;
}

- (void)sizeMenuItemChosen:(NSMenuItem *)item {
    [self setDiameter:static_cast<CGFloat>(item.tag)];
}

- (void)setDiameter:(CGFloat)diameter {
    const NSRect frame = self.frame;
    const NSRect resized = NSMakeRect(NSMidX(frame) - diameter / 2.0, NSMidY(frame) - diameter / 2.0,
                                      diameter, diameter);
    [self setFrame:resized display:YES];
    [self layoutCircleForDiameter:diameter];
}

/// Sizes both layers and turns the square into a circle (corner radius = half the side).
/// Implicit animations are disabled so resizing doesn't "wobble".
- (void)layoutCircleForDiameter:(CGFloat)diameter {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    const CGRect bounds = CGRectMake(0, 0, diameter, diameter);
    _circleLayer.frame = bounds;
    _circleLayer.cornerRadius = diameter / 2.0;
    _previewLayer.frame = bounds;
    [CATransaction commit];
}

- (void)placeOnScreen:(NSScreen *)screen {
    if (screen == nil) {
        return;
    }
    const BOOL alreadyThere = self.isVisible && self.screen != nil &&
                              [DeviceCatalog displayIDForScreen:self.screen] == [DeviceCatalog displayIDForScreen:screen];
    if (alreadyThere) {
        return;
    }
    const NSRect visible = screen.visibleFrame;  // excludes menu bar and Dock
    [self setFrameOrigin:NSMakePoint(NSMinX(visible) + kScreenMargin, NSMinY(visible) + kScreenMargin)];
}

@end
