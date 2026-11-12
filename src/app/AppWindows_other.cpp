// Platforms without window details (e.g. Linux without X11): callers keep Qt's own list.
#include "app/AppWindows.hpp"

QHash<quintptr, AppWindow> listAppWindows(bool *supported) {
    if (supported != nullptr) {
        *supported = false;
    }
    return {};
}
