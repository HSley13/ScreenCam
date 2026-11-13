// Qt's winId() on macOS is the window's NSView.
#import <AppKit/AppKit.h>

#include <QWindow>

bool screencamExcludeNativeWindowFromCapture(WId windowId) {
    NSView *view = reinterpret_cast<NSView *>(windowId);
    if (view == nil || view.window == nil) {
        return false;  // not shown yet
    }
    view.window.sharingType = NSWindowSharingNone;
    return true;
}
