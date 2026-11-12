#import "app/AppDelegate.h"

#import <AVFoundation/AVFoundation.h>

#import "platform/CameraCapture.h"
#import "platform/DeviceCatalog.h"
#import "ui/CameraBubbleWindow.h"
#import "ui/ControlPanelController.h"

namespace {

constexpr CGFloat kDefaultBubbleDiameter = 180.0;

BOOL SameID(NSString *_Nullable a, NSString *_Nullable b) {
    return (a == nil && b == nil) || [a isEqualToString:b];
}

/// Builds the menu bar in code (we don't use a .xib/.storyboard).
/// Without an app menu there would be no ⌘Q and no "Quit" item.
NSMenu *BuildMainMenu() {
    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];

    NSMenuItem *appItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"ScreenCam"];
    [appMenu addItemWithTitle:@"About ScreenCam" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [appMenu addItem:NSMenuItem.separatorItem];
    [appMenu addItemWithTitle:@"Hide ScreenCam" action:@selector(hide:) keyEquivalent:@"h"];
    [appMenu addItem:NSMenuItem.separatorItem];
    [appMenu addItemWithTitle:@"Quit ScreenCam" action:@selector(terminate:) keyEquivalent:@"q"];
    appItem.submenu = appMenu;
    [mainMenu addItem:appItem];

    NSMenuItem *windowItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"];
    windowItem.submenu = windowMenu;
    [mainMenu addItem:windowItem];
    NSApp.windowsMenu = windowMenu;

    return mainMenu;
}

/// Brings the app to the front. `activateIgnoringOtherApps:` is deprecated on macOS 14+.
void ActivateApp() {
    if (@available(macOS 14.0, *)) {
        [NSApp activate];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
    }
}

}  // namespace

@interface AppDelegate () <ControlPanelDelegate>
@end

@implementation AppDelegate {
    ControlPanelController *_panel;
    CameraCapture *_camera;        // one shared camera session (preview + recording frames)
    CameraBubbleWindow *_bubble;   // the circle you drag around
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSApp.mainMenu = BuildMainMenu();

    _camera = [[CameraCapture alloc] init];
    _bubble = [[CameraBubbleWindow alloc] initWithPreviewLayer:_camera.previewLayer diameter:kDefaultBubbleDiameter];

    _panel = [[ControlPanelController alloc] init];
    _panel.delegate = self;
    [_panel showWindow:nil];
    ActivateApp();

    [self observeSystemChanges];
    [self requestPermissions];
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    return YES;
}

/// Keeps the pickers in sync with the hardware and quits when the panel is closed.
- (void)observeSystemChanges {
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    NSOperationQueue *main = NSOperationQueue.mainQueue;
    __weak AppDelegate *weakSelf = self;
    void (^refresh)(NSNotification *) = ^(NSNotification *note) {
        [weakSelf sourcesDidChange];
    };
    [center addObserverForName:AVCaptureDeviceWasConnectedNotification object:nil queue:main usingBlock:refresh];
    [center addObserverForName:AVCaptureDeviceWasDisconnectedNotification object:nil queue:main usingBlock:refresh];
    [center addObserverForName:NSApplicationDidChangeScreenParametersNotification object:nil queue:main usingBlock:refresh];

    // The panel *is* the app: closing it quits (the camera bubble alone isn't useful).
    [center addObserverForName:NSWindowWillCloseNotification
                        object:_panel.window
                         queue:main
                    usingBlock:^(NSNotification *note) {
                        [NSApp terminate:nil];
                    }];
}

- (void)sourcesDidChange {
    [_panel reloadSources];
    [self applySelection];
}

#pragma mark - Permissions

/// Asks for camera, then microphone access (one prompt at a time), then reports what's missing.
/// macOS only shows each prompt once; after a "Don't Allow" the user must use System Settings.
- (void)requestPermissions {
    __weak AppDelegate *weakSelf = self;
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL cameraGranted) {
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL micGranted) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [weakSelf permissionsResolvedCamera:cameraGranted microphone:micGranted];
            });
        }];
    }];
}

- (void)permissionsResolvedCamera:(BOOL)cameraGranted microphone:(BOOL)micGranted {
    NSMutableArray<NSString *> *missing = [NSMutableArray array];
    if (!cameraGranted) {
        [missing addObject:@"Camera"];
    }
    if (!micGranted) {
        [missing addObject:@"Microphone"];
    }
    if (missing.count > 0) {
        [_panel setStatusText:[NSString stringWithFormat:@"Allow %@ for ScreenCam in System Settings ▸ Privacy & Security.",
                                                         [missing componentsJoinedByString:@" and "]]];
    }
    [self applySelection];
}

#pragma mark - Selection

/// Makes the camera session and the bubble match the panel:
///  - camera changed  -> switch the capture session (async),
///  - camera "None"   -> hide the bubble,
///  - screen changed  -> move the bubble onto that screen so it ends up in the recording.
- (void)applySelection {
    const BOOL cameraAllowed =
        [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo] == AVAuthorizationStatusAuthorized;
    NSString *wantedCamera = cameraAllowed ? _panel.selectedCameraID : nil;

    if (!SameID(wantedCamera, _camera.currentDeviceID)) {
        __weak AppDelegate *weakSelf = self;
        [_camera switchToDeviceWithUniqueID:wantedCamera completion:^(NSError *error) {
            if (error != nil) {
                [weakSelf showStatus:error.localizedDescription];
            }
        }];
    }

    if (wantedCamera != nil) {
        [_bubble placeOnScreen:[DeviceCatalog screenForDisplayID:_panel.selectedDisplayID]];
        [_bubble orderFrontRegardless];
    } else {
        [_bubble orderOut:nil];
    }
}

- (void)showStatus:(NSString *)text {
    [_panel setStatusText:text];
}

#pragma mark - ControlPanelDelegate

- (void)controlPanelSelectionDidChange:(ControlPanelController *)panel {
    [self applySelection];
}

- (void)controlPanelDidRequestToggleRecording:(ControlPanelController *)panel {
    [panel setStatusText:@"Recording isn't implemented yet."];
}

- (void)controlPanelDidRequestReveal:(ControlPanelController *)panel {
}

@end
