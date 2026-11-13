#!/usr/bin/env bash
# Installs a self-contained Release build, signed with "ScreenCam Development" so granted
# permissions carry over. Run it again to update.
#
#   scripts/install.sh                      # macOS: ~/Applications, Linux: ~/.local
#   PREFIX=/Applications scripts/install.sh # somewhere else
set -euo pipefail
cd "$(dirname "$0")/.."

step() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }
die() { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }

case "$(uname -s)" in
    Darwin) PREFIX="${PREFIX:-$HOME/Applications}" ;;
    Linux) PREFIX="${PREFIX:-$HOME/.local}" ;;
    *) die "On Windows, see README > Windows (cmake --install with a prefix of your choice)." ;;
esac

# The same Qt as build/, unless CMAKE_PREFIX_PATH says otherwise.
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
if [[ "$(uname -s)" == "Darwin" ]]; then
    # macdeployqt is silent for minutes, which looks like a hang.
    echo "This step takes about 3 minutes and shows no progress while it works — let it run."
fi
mkdir -p "$PREFIX"
if [[ "$(uname -s)" == "Darwin" ]]; then
    # Removed files would linger in a copy over the old one.
    rm -rf "$PREFIX/ScreenCam.app"
fi
cmake --install build/release --prefix "$PREFIX"

if [[ "$(uname -s)" == "Darwin" ]]; then
    app="$PREFIX/ScreenCam.app"
    [[ -d "$app" ]] || die "Install finished but $app is missing."
    codesign --verify --deep "$app" || die "The installed app's signature is broken (see README > Permissions)."
    # Now, so Spotlight finds it and .screencam projects open with it.
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
