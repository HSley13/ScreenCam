#import "app/AppDelegate.h"

#import <AVFoundation/AVFoundation.h>

#import "ui/ControlPanelController.h"

namespace {

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
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSApp.mainMenu = BuildMainMenu();

    _panel = [[ControlPanelController alloc] init];
    _panel.delegate = self;
    [_panel showWindow:nil];
    ActivateApp();

    [self observeSystemChanges];
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

/// Applies the panel's current choices. Filled in by later features (camera bubble, recording).
- (void)applySelection {
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
