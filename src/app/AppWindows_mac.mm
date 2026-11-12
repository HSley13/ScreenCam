// macOS: the window server's list (CGWindowListCopyWindowInfo) plus NSRunningApplication.
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include "app/AppWindows.hpp"
#include "core/WindowFilter.hpp"

#include <unistd.h>

namespace {

// The keys are CFStringRef constants; NSDictionary wants NSString keys (toll-free bridged).
id value(NSDictionary *info, CFStringRef key) {
    return info[(__bridge NSString *)key];
}

}  // namespace

QHash<quintptr, AppWindow> listAppWindows(bool *supported) {
    if (supported != nullptr) {
        *supported = true;
    }
    QHash<quintptr, AppWindow> result;
    @autoreleasepool {
        // On-screen windows only (that's also what Qt lists and what can be captured: a
        // minimized window or one on another Space has no pixels to record), without the
        // desktop picture and desktop icons.
        CFArrayRef list = CGWindowListCopyWindowInfo(
            kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
        if (list == nullptr) {
            return result;
        }
        NSArray *windows = (__bridge NSArray *)list;
        const pid_t ownPid = getpid();

        for (NSDictionary *info in windows) {
            screencam::core::WindowTraits traits;
            // Level 0 = normal windows. Menu bar (24), status items (25), Dock (20),
            // pop-up menus (101), screen savers ... are all above it.
            traits.layer = [value(info, kCGWindowLayer) intValue];
            NSNumber *alpha = value(info, kCGWindowAlpha);
            traits.alpha = alpha != nil ? alpha.doubleValue : 1.0;

            CGRect bounds = CGRectZero;
            NSDictionary *boundsInfo = value(info, kCGWindowBounds);
            if (boundsInfo != nil) {
                CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)boundsInfo, &bounds);
            }
            traits.width = static_cast<int>(bounds.size.width);
            traits.height = static_cast<int>(bounds.size.height);

            const pid_t pid = [value(info, kCGWindowOwnerPID) intValue];
            traits.ownedBySelf = pid == ownPid;
            // "Regular" apps are the ones with a Dock icon and a menu bar. Menu-bar utilities
            // (CPU/battery meters, clipboard managers) are "accessory" apps, and system parts
            // (Control Center, Notification Center, Spotlight) are background agents — their
            // pop-overs can be big, level-0 windows, so the level alone isn't enough.
            NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
            traits.regularApp = app != nil && app.activationPolicy == NSApplicationActivationPolicyRegular;
            // Every level-0 window of a regular app has the standard window buttons; macOS
            // doesn't report them separately, so hasWindowControls keeps its default (true).

            if (!screencam::core::isUserAppWindow(traits)) {
                continue;
            }

            AppWindow window;
            window.id = static_cast<quintptr>([value(info, kCGWindowNumber) unsignedIntValue]);
            NSString *appName = app.localizedName;
            if (appName == nil) {
                appName = value(info, kCGWindowOwnerName);
            }
            window.appName = QString::fromNSString(appName);
            // Titles are only visible once Screen Recording is allowed; until then, the
            // label is just the app name.
            window.title = QString::fromNSString(value(info, kCGWindowName));
            window.label = QString::fromStdString(
                screencam::core::windowLabel(window.appName.toStdString(), window.title.toStdString()));
            result.insert(window.id, window);
        }
        CFRelease(list);
    }
    return result;
}
