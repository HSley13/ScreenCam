#import "app/AppDelegate.h"

#import <AVFoundation/AVFoundation.h>

#import "platform/Bridging.h"
#import "platform/CameraCapture.h"
#import "platform/DeviceCatalog.h"
#import "recording/RecordingSession.h"
#import "ui/CameraBubbleWindow.h"
#import "ui/ControlPanelController.h"

#include "core/TimeFormat.hpp"

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
    RecordingSession *_recording;  // non-nil from "Start" until the files are saved
    NSTimer *_elapsedTimer;
    NSDate *_recordingStartedAt;
    NSArray<NSURL *> *_lastRecordingFiles;
    BOOL _stopInProgress;
    BOOL _terminateWhenStopped;    // user quit while recording: finish the files first
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

/// Quitting mid-recording would leave unreadable .mov files (the index is written at the end),
/// so we stop cleanly first and only then let the app terminate.
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    if (_recording == nil) {
        return NSTerminateNow;
    }
    _terminateWhenStopped = YES;
    [self stopRecording];
    return NSTerminateLater;
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
    if (!CGPreflightScreenCaptureAccess()) {
        CGRequestScreenCaptureAccess();  // shows the system prompt / adds us to the Settings list
        [missing addObject:@"Screen Recording"];
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
    if (_recording != nil) {
        return;  // pickers are locked while recording; never swap the camera mid-take
    }
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
    if (_recording != nil) {
        [self stopRecording];
    } else {
        [self startRecording];
    }
}

- (void)controlPanelDidRequestReveal:(ControlPanelController *)panel {
    if (_lastRecordingFiles.count > 0) {
        [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:_lastRecordingFiles];
    }
}

#pragma mark - Recording

/// Snapshots the panel into RecordingOptions and starts a RecordingSession.
- (void)startRecording {
    if (!CGPreflightScreenCaptureAccess()) {
        CGRequestScreenCaptureAccess();
        [self showScreenPermissionAlert];
        return;
    }

    RecordingOptions *options = [[RecordingOptions alloc] init];
    options.displayID = _panel.selectedDisplayID;
    options.resolution = _panel.selectedResolution;
    options.cameraOrientation = _panel.selectedOrientation;
    options.recordCamera = (_panel.selectedCameraID != nil && _camera.currentDeviceID != nil);
    options.microphoneID = _panel.selectedMicrophoneID;
    // Hide the control panel from the video; the bubble is NOT excluded, so it is recorded.
    options.excludedWindowNumbers = @[@(_panel.window.windowNumber)];

    RecordingSession *session = [[RecordingSession alloc] initWithOptions:options camera:_camera];
    __weak AppDelegate *weakSelf = self;
    session.onUnexpectedStop = ^(NSError *error) {
        NSLog(@"ScreenCam: capture stopped by the system: %@", error);
        [weakSelf stopRecording];
    };

    _recording = session;
    [_panel setRecording:YES];
    [_panel setBusy:YES];
    [_panel setRevealEnabled:NO];
    [self showStatus:@"Starting…"];

    [session startWithCompletion:^(NSError *error) {
        [weakSelf recordingSession:session didStartWithError:error];
    }];
}

- (void)recordingSession:(RecordingSession *)session didStartWithError:(NSError *)error {
    if (session != _recording) {
        return;  // a stale callback (e.g. the user quit while starting)
    }
    [_panel setBusy:NO];
    if (error != nil) {
        _recording = nil;
        [_panel setRecording:NO];
        [self showStatus:error.localizedDescription];
        if (_terminateWhenStopped) {
            [NSApp replyToApplicationShouldTerminate:YES];
        }
        return;
    }
    _recordingStartedAt = [NSDate date];
    [self updateElapsed];
    __weak AppDelegate *weakSelf = self;
    _elapsedTimer = [NSTimer scheduledTimerWithTimeInterval:0.5
                                                    repeats:YES
                                                      block:^(NSTimer *timer) {
                                                          [weakSelf updateElapsed];
                                                      }];
}

- (void)stopRecording {
    RecordingSession *session = _recording;
    if (session == nil || _stopInProgress) {
        return;
    }
    _stopInProgress = YES;
    [_elapsedTimer invalidate];
    _elapsedTimer = nil;
    _recordingStartedAt = nil;
    [_panel setBusy:YES];
    [self showStatus:@"Saving…"];

    __weak AppDelegate *weakSelf = self;
    [session stopWithCompletion:^(NSArray<NSURL *> *files, NSError *error) {
        [weakSelf recordingDidStopWithFiles:files error:error];
    }];
}

- (void)recordingDidStopWithFiles:(NSArray<NSURL *> *)files error:(NSError *)error {
    _recording = nil;
    _stopInProgress = NO;
    _lastRecordingFiles = files;
    [_panel setBusy:NO];
    [_panel setRecording:NO];
    [_panel setRevealEnabled:files.count > 0];

    NSString *status = (files.count > 0)
        ? [NSString stringWithFormat:@"Saved %lu file%@ to Movies ▸ ScreenCam.", static_cast<unsigned long>(files.count),
                                     files.count == 1 ? @"" : @"s"]
        : @"Nothing was saved.";
    if (error != nil) {
        status = [status stringByAppendingFormat:@" (%@)", error.localizedDescription];
    }
    [self showStatus:status];

    if (_terminateWhenStopped) {
        [NSApp replyToApplicationShouldTerminate:YES];
    } else {
        [self applySelection];  // pick up any device change that happened while recording
    }
}

- (void)updateElapsed {
    if (_recordingStartedAt == nil) {
        return;
    }
    const double seconds = -[_recordingStartedAt timeIntervalSinceNow];
    [self showStatus:[NSString stringWithFormat:@"● Recording  %@", SCString(screencam::core::formatElapsed(seconds))]];
}

- (void)showScreenPermissionAlert {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Screen Recording permission needed";
    alert.informativeText = @"Turn on ScreenCam in System Settings ▸ Privacy & Security ▸ Screen & System Audio "
                            @"Recording, then quit and reopen ScreenCam.";
    [alert addButtonWithTitle:@"Open System Settings"];
    [alert addButtonWithTitle:@"OK"];
    if ([alert runModal] == NSAlertFirstButtonReturn) {
        NSURL *url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture"];
        [NSWorkspace.sharedWorkspace openURL:url];
    }
}

@end
