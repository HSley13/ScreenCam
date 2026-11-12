#import "ui/ControlPanelController.h"

#import "platform/Bridging.h"
#import "platform/DeviceCatalog.h"

using screencam::core::CameraOrientation;
using screencam::core::ResolutionPreset;

namespace {

constexpr CGFloat kPopupMinWidth = 260.0;
constexpr CGFloat kStatusMaxWidth = 380.0;

NSTextField *MakeRowLabel(NSString *text) {
    NSTextField *label = [NSTextField labelWithString:text];
    label.alignment = NSTextAlignmentRight;
    return label;
}

/// Adds an item through the menu instead of -addItemWithTitle:, because the latter silently
/// replaces an existing item with the same title (two identical USB webcams would collide).
/// `representedObject` carries the device id; nil means "None".
void AddMenuItem(NSPopUpButton *popup, NSString *title, id _Nullable representedObject) {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
    item.representedObject = representedObject;
    [popup.menu addItem:item];
}

/// Selects the first item whose representedObject equals `value`. Returns NO if none matched.
BOOL SelectItemWithObject(NSPopUpButton *popup, id _Nullable value) {
    if (value == nil) {
        return NO;
    }
    for (NSMenuItem *item in popup.itemArray) {
        if ([item.representedObject isEqual:value]) {
            [popup selectItem:item];
            return YES;
        }
    }
    return NO;
}

/// Refills a "None + devices" pop-up. On the very first fill it picks the system default
/// device; afterwards it keeps whatever the user had (falling back to "None" if it vanished).
void FillDevicePopup(NSPopUpButton *popup, NSArray<SourceInfo *> *sources, NSString *_Nullable defaultID) {
    const BOOL firstFill = (popup.numberOfItems == 0);
    NSString *previous = popup.selectedItem.representedObject;

    [popup removeAllItems];
    AddMenuItem(popup, @"None", nil);
    for (SourceInfo *source in sources) {
        AddMenuItem(popup, source.name, source.uniqueID);
    }

    if (firstFill) {
        if (!SelectItemWithObject(popup, defaultID) && popup.numberOfItems > 1) {
            [popup selectItemAtIndex:1];
        }
    } else if (!SelectItemWithObject(popup, previous)) {
        [popup selectItemAtIndex:0];
    }
}

}  // namespace

@implementation ControlPanelController {
    NSPopUpButton *_displayPopup;
    NSPopUpButton *_resolutionPopup;
    NSPopUpButton *_cameraPopup;
    NSSegmentedControl *_orientationControl;
    NSPopUpButton *_micPopup;
    NSButton *_recordButton;
    NSButton *_revealButton;
    NSTextField *_statusLabel;
}

- (instancetype)init {
    NSWindow *window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 440, 320)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window.title = @"ScreenCam";
    window.releasedWhenClosed = NO;

    self = [super initWithWindow:window];
    if (self) {
        [self buildInterface];
        [self reloadSources];
        [window center];
    }
    return self;
}

#pragma mark - UI construction

- (NSPopUpButton *)makePopup {
    NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    popup.autoenablesItems = NO;  // our items have no action; keep them all enabled
    popup.target = self;
    popup.action = @selector(selectionChanged:);
    [popup.widthAnchor constraintGreaterThanOrEqualToConstant:kPopupMinWidth].active = YES;
    return popup;
}

- (void)buildInterface {
    _displayPopup = [self makePopup];

    _resolutionPopup = [self makePopup];
    for (const auto &option : screencam::core::resolutionOptions()) {
        AddMenuItem(_resolutionPopup, SCString(option.label), nil);
        _resolutionPopup.lastItem.tag = static_cast<NSInteger>(option.preset);
    }
    [_resolutionPopup selectItemWithTag:static_cast<NSInteger>(ResolutionPreset::FullHD1080)];

    _cameraPopup = [self makePopup];
    _micPopup = [self makePopup];

    _orientationControl = [NSSegmentedControl segmentedControlWithLabels:@[@"Landscape", @"Portrait"]
                                                            trackingMode:NSSegmentSwitchTrackingSelectOne
                                                                  target:self
                                                                  action:@selector(selectionChanged:)];
    _orientationControl.selectedSegment = 0;

    NSGridView *grid = [NSGridView gridViewWithViews:@[
        @[MakeRowLabel(@"Screen:"), _displayPopup],
        @[MakeRowLabel(@"Resolution:"), _resolutionPopup],
        @[MakeRowLabel(@"Camera:"), _cameraPopup],
        @[MakeRowLabel(@"Camera mode:"), _orientationControl],
        @[MakeRowLabel(@"Microphone:"), _micPopup],
    ]];
    grid.rowSpacing = 10;
    grid.columnSpacing = 10;
    grid.rowAlignment = NSGridRowAlignmentFirstBaseline;
    [grid columnAtIndex:0].xPlacement = NSGridCellPlacementTrailing;

    _recordButton = [NSButton buttonWithTitle:@"Start Recording" target:self action:@selector(recordPressed:)];
    _recordButton.keyEquivalent = @"\r";  // Return starts/stops; also makes it the blue default button
    _revealButton = [NSButton buttonWithTitle:@"Show in Finder" target:self action:@selector(revealPressed:)];
    _revealButton.enabled = NO;
    NSStackView *buttons = [NSStackView stackViewWithViews:@[_recordButton, _revealButton]];
    buttons.spacing = 8;

    _statusLabel = [NSTextField wrappingLabelWithString:@"Ready"];
    _statusLabel.font = [NSFont monospacedDigitSystemFontOfSize:13 weight:NSFontWeightMedium];
    _statusLabel.alignment = NSTextAlignmentCenter;
    _statusLabel.preferredMaxLayoutWidth = kStatusMaxWidth;

    NSTextField *hint = [NSTextField wrappingLabelWithString:
        @"This window is hidden from recordings. Drag the camera bubble anywhere; right-click it to resize."];
    hint.font = [NSFont systemFontOfSize:11];
    hint.textColor = NSColor.secondaryLabelColor;
    hint.alignment = NSTextAlignmentCenter;
    hint.preferredMaxLayoutWidth = kStatusMaxWidth;

    NSStackView *root = [NSStackView stackViewWithViews:@[grid, buttons, _statusLabel, hint]];
    root.orientation = NSUserInterfaceLayoutOrientationVertical;
    root.alignment = NSLayoutAttributeCenterX;
    root.spacing = 14;
    root.edgeInsets = NSEdgeInsetsMake(20, 20, 20, 20);
    root.translatesAutoresizingMaskIntoConstraints = NO;

    NSView *content = self.window.contentView;
    [content addSubview:root];
    [NSLayoutConstraint activateConstraints:@[
        [root.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
        [root.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
        [root.topAnchor constraintEqualToAnchor:content.topAnchor],
        [root.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
    ]];
    [self.window setContentSize:root.fittingSize];
}

#pragma mark - Sources

- (void)reloadSources {
    NSNumber *previousDisplay = _displayPopup.selectedItem.representedObject;
    [_displayPopup removeAllItems];
    for (DisplayInfo *display in DeviceCatalog.displays) {
        NSString *title = [NSString stringWithFormat:@"%@ — %d × %d", display.name,
                                                     display.pixelSize.width, display.pixelSize.height];
        AddMenuItem(_displayPopup, title, @(display.displayID));
    }
    if (!SelectItemWithObject(_displayPopup, previousDisplay)) {
        SelectItemWithObject(_displayPopup, @(CGMainDisplayID()));
    }

    FillDevicePopup(_cameraPopup, DeviceCatalog.cameras, DeviceCatalog.defaultCameraID);
    FillDevicePopup(_micPopup, DeviceCatalog.microphones, DeviceCatalog.defaultMicrophoneID);
}

#pragma mark - Selection getters

- (CGDirectDisplayID)selectedDisplayID {
    NSNumber *number = _displayPopup.selectedItem.representedObject;
    return (number != nil) ? static_cast<CGDirectDisplayID>(number.unsignedIntValue) : kCGNullDirectDisplay;
}

- (NSString *)selectedCameraID {
    return _cameraPopup.selectedItem.representedObject;
}

- (NSString *)selectedMicrophoneID {
    return _micPopup.selectedItem.representedObject;
}

- (ResolutionPreset)selectedResolution {
    return static_cast<ResolutionPreset>(_resolutionPopup.selectedItem.tag);
}

- (CameraOrientation)selectedOrientation {
    return (_orientationControl.selectedSegment == 1) ? CameraOrientation::Portrait : CameraOrientation::Landscape;
}

#pragma mark - State

- (void)setRecording:(BOOL)recording {
    _recordButton.title = recording ? @"Stop Recording" : @"Start Recording";
    for (NSControl *control in @[_displayPopup, _resolutionPopup, _cameraPopup, _orientationControl, _micPopup]) {
        control.enabled = !recording;
    }
}

- (void)setBusy:(BOOL)busy {
    _recordButton.enabled = !busy;
}

- (void)setStatusText:(NSString *)text {
    _statusLabel.stringValue = text;
}

- (void)setRevealEnabled:(BOOL)enabled {
    _revealButton.enabled = enabled;
}

#pragma mark - Actions

- (void)selectionChanged:(id)sender {
    [self.delegate controlPanelSelectionDidChange:self];
}

- (void)recordPressed:(id)sender {
    [self.delegate controlPanelDidRequestToggleRecording:self];
}

- (void)revealPressed:(id)sender {
    [self.delegate controlPanelDidRequestReveal:self];
}

@end
