// Entry point: create the shared NSApplication, attach our delegate and run the event loop.
#import <AppKit/AppKit.h>

#import "app/AppDelegate.h"

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];

        // NSApplication.delegate is a *weak* reference, so this local strong variable is
        // what keeps the delegate alive. [app run] never returns (terminate: calls exit()).
        AppDelegate *delegate = [[AppDelegate alloc] init];
        app.delegate = delegate;

        // A regular app: Dock icon + menu bar. Needed because we have no Info.plist-driven nib.
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app run];
    }
    return 0;
}
