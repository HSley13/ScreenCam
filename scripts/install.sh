#!/usr/bin/env bash
# Installs ScreenCam as a normal app: optimized (Release) build, with Qt and FFmpeg copied
# inside so it runs on its own, signed with your "ScreenCam Development" identity (so the
# Camera / Microphone / Screen Recording permissions you granted carry over).
#
#   scripts/install.sh                      # macOS: ~/Applications/ScreenCam.app
#                                           # Linux: ~/.local (app menu entry + icon)
#   PREFIX=/Applications scripts/install.sh # somewhere else
#
# Run it again after pulling changes: it replaces the installed copy.
set -euo pipefail
cd "$(dirname "$0")/.."

step() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }
die() { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }

case "$(uname -s)" in
    Darwin) PREFIX="${PREFIX:-$HOME/Applications}" ;;
    Linux) PREFIX="${PREFIX:-$HOME/.local}" ;;
    *) die "On Windows, see README > Windows (cmake --install with a prefix of your choice)." ;;
esac

# Use the same Qt as your day-to-day build in build/ (so you don't have to tell CMake where
# Qt is again), unless CMAKE_PREFIX_PATH already says.
configure_args=(-DSCREENCAM_BUILD_TESTS=OFF)
if [[ -z "${CMAKE_PREFIX_PATH:-}" && -f build/CMakeCache.txt ]]; then
    qt_dir="$(sed -n 's/^Qt6_DIR:PATH=//p' build/CMakeCache.txt)"
    if [[ -n "$qt_dir" ]]; then
        configure_args+=("-DQt6_DIR=$qt_dir")
    fi
fi

step "Configuring a Release build in build/release"
cmake --preset release "${configure_args[@]}"

step "Building"
cmake --build --preset release

step "Installing into $PREFIX"
mkdir -p "$PREFIX"
if [[ "$(uname -s)" == "Darwin" ]]; then
    # Replace the previous copy instead of copying over it (removed files would linger).
    rm -rf "$PREFIX/ScreenCam.app"
fi
cmake --install build/release --prefix "$PREFIX"

if [[ "$(uname -s)" == "Darwin" ]]; then
    app="$PREFIX/ScreenCam.app"
    [[ -d "$app" ]] || die "Install finished but $app is missing."
    codesign --verify --deep "$app" || die "The installed app's signature is broken (see README > Permissions)."
    # Tell macOS about the app now: Spotlight/Launchpad find it, and .screencam projects open
    # with it on double-click.
    lsregister=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
    if [[ -x "$lsregister" ]]; then
        "$lsregister" -f "$app"
    fi
    step "Done"
    echo "Open ScreenCam from Spotlight (⌘Space → ScreenCam), Launchpad, or Finder ▸ $PREFIX."
    echo "Tip: while it's open, right-click its Dock icon ▸ Options ▸ Keep in Dock."
else
    command -v update-desktop-database >/dev/null && update-desktop-database "$PREFIX/share/applications" || true
    step "Done"
    echo "ScreenCam is in your app menu, or run: $PREFIX/bin/ScreenCam"
    case ":$PATH:" in
        *":$PREFIX/bin:"*) ;;
        *) echo "(Add $PREFIX/bin to PATH to start it as 'ScreenCam' from a terminal.)" ;;
    esac
fi
