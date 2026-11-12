// Windows: EnumWindows with the same rules the taskbar / Alt+Tab use.
#include "app/AppWindows.hpp"
#include "core/WindowFilter.hpp"

#include <QFileInfo>

#include <vector>

#include <windows.h>
#include <dwmapi.h>  // DwmGetWindowAttribute(DWMWA_CLOAKED)

namespace {

/// "Google Chrome" from chrome.exe's version resource (what Task Manager shows), or "".
QString fileDescription(const QString &exePath) {
    const std::wstring path = exePath.toStdWString();
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0) {
        return {};
    }
    std::vector<char> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) {
        return {};
    }
    struct Translation {
        WORD language;
        WORD codePage;
    };
    Translation *translations = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void **>(&translations),
                        &length) ||
        length < sizeof(Translation)) {
        return {};
    }
    const std::wstring key = QStringLiteral("\\StringFileInfo\\%1%2\\FileDescription")
                                 .arg(translations[0].language, 4, 16, QLatin1Char('0'))
                                 .arg(translations[0].codePage, 4, 16, QLatin1Char('0'))
                                 .toStdWString();
    wchar_t *text = nullptr;
    UINT textLength = 0;
    if (!VerQueryValueW(data.data(), key.c_str(), reinterpret_cast<void **>(&text), &textLength) ||
        textLength == 0) {
        return {};
    }
    return QString::fromWCharArray(text).trimmed();
}

/// Display name of the app that owns a process.
QString appNameOf(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return {};
    }
    wchar_t buffer[MAX_PATH * 2] = {};
    DWORD size = static_cast<DWORD>(std::size(buffer));
    QString name;
    if (QueryFullProcessImageNameW(process, 0, buffer, &size)) {
        const QString exe = QString::fromWCharArray(buffer, static_cast<int>(size));
        const QString baseName = QFileInfo(exe).completeBaseName();
        // Store apps (Calculator, Photos ...) are hosted by ApplicationFrameHost.exe; their
        // window title already is the app name, so don't prefix "Application Frame Host".
        if (baseName.compare(QStringLiteral("ApplicationFrameHost"), Qt::CaseInsensitive) != 0) {
            name = fileDescription(exe);
            if (name.isEmpty()) {
                name = baseName;
            }
        }
    }
    CloseHandle(process);
    return name;
}

struct Context {
    QHash<quintptr, AppWindow> *result = nullptr;
    DWORD ownPid = 0;
};

BOOL CALLBACK collectWindow(HWND hwnd, LPARAM param) {
    auto *context = reinterpret_cast<Context *>(param);

    screencam::core::WindowTraits traits;
    traits.visible = IsWindowVisible(hwnd) != FALSE;
    const int titleLength = GetWindowTextLengthW(hwnd);
    if (!traits.visible || titleLength == 0) {
        return TRUE;  // invisible helpers and untitled windows never show in the taskbar
    }

    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    // The taskbar rule: a window gets a button unless it's a tool window, a non-activating
    // overlay, or owned by another window (dialogs, pop-ups) — WS_EX_APPWINDOW overrides.
    const bool forcedAppWindow = (exStyle & WS_EX_APPWINDOW) != 0;
    const bool owned = GetWindow(hwnd, GW_OWNER) != nullptr;
    traits.toolWindow =
        !forcedAppWindow && ((exStyle & WS_EX_TOOLWINDOW) != 0 || (exStyle & WS_EX_NOACTIVATE) != 0 || owned);
    traits.hasWindowControls =
        (style & WS_CAPTION) == WS_CAPTION || (style & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) != 0;

    // "Cloaked" windows are on another virtual desktop, or suspended Store apps: listed as
    // visible but not actually on screen.
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
        traits.cloaked = cloaked != 0;
    }

    // A minimized window reports a tiny rectangle far off screen; use its normal size.
    RECT rect{};
    if (IsIconic(hwnd)) {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        GetWindowPlacement(hwnd, &placement);
        rect = placement.rcNormalPosition;
    } else {
        GetWindowRect(hwnd, &rect);
    }
    traits.width = rect.right - rect.left;
    traits.height = rect.bottom - rect.top;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    traits.ownedBySelf = pid == context->ownPid;

    if (!screencam::core::isUserAppWindow(traits)) {
        return TRUE;
    }

    std::wstring title(static_cast<std::size_t>(titleLength) + 1, L'\0');
    GetWindowTextW(hwnd, title.data(), titleLength + 1);

    AppWindow window;
    window.id = reinterpret_cast<quintptr>(hwnd);
    window.appName = appNameOf(pid);
    window.title = QString::fromWCharArray(title.c_str());
    window.label =
        QString::fromStdString(screencam::core::windowLabel(window.appName.toStdString(), window.title.toStdString()));
    context->result->insert(window.id, window);
    return TRUE;
}

}  // namespace

QHash<quintptr, AppWindow> listAppWindows(bool *supported) {
    if (supported != nullptr) {
        *supported = true;
    }
    QHash<quintptr, AppWindow> result;
    Context context;
    context.result = &result;
    context.ownPid = GetCurrentProcessId();
    EnumWindows(collectWindow, reinterpret_cast<LPARAM>(&context));
    return result;
}
