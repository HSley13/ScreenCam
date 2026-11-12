#import "app/AppDelegate.h"

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

@implementation AppDelegate {
    NSWindow *_window;  // placeholder until the control panel exists
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSApp.mainMenu = BuildMainMenu();

    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 420, 200)
                                          styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    _window.title = @"ScreenCam";
    _window.releasedWhenClosed = NO;
    [_window center];
    [_window makeKeyAndOrderFront:nil];
    ActivateApp();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    return YES;
}

@end
