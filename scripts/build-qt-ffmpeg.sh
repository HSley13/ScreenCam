#!/usr/bin/env bash
# Adds Qt Multimedia's FFmpeg backend to a Qt you built from source (macOS).
#
# Why: QScreenCapture and QVideoFrameInput — which ScreenCam needs for the screen preview and
# for recording — only exist in Qt Multimedia's FFmpeg backend. A Qt configured without
# -ffmpeg-dir ships only the native backend (plugins/multimedia/libdarwinmediaplugin.dylib).
#
# What it does:
#   1. builds FFmpeg (shared libs, no external codecs: H.264 encoding uses Apple VideoToolbox)
#      into $FFMPEG_PREFIX (default ~/.local/ffmpeg-<version>, kept: the plugin links to it),
#   2. downloads the Qt Multimedia sources matching your Qt version,
#   3. reconfigures them against your Qt with FFmpeg enabled + deployed, builds, installs,
#   4. checks that plugins/multimedia/libffmpegmediaplugin.dylib now exists.
#
# Usage:   scripts/build-qt-ffmpeg.sh
# Options (environment): QT_DIR=/usr/local/Qt-6.10.0  FFMPEG_VERSION=7.1.3  JOBS=<cores>
# Re-running is safe: finished steps are skipped. Log: .deps/qt-ffmpeg/build.log
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QT_DIR="${QT_DIR:-/usr/local/Qt-6.10.0}"
FFMPEG_VERSION="${FFMPEG_VERSION:-7.1.3}"
FFMPEG_PREFIX="${FFMPEG_PREFIX:-$HOME/.local/ffmpeg-$FFMPEG_VERSION}"
WORK_DIR="${WORK_DIR:-$REPO_DIR/.deps/qt-ffmpeg}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
LOG="$WORK_DIR/build.log"

step() { printf '\n==> [%s] %s\n' "$(date +%H:%M:%S)" "$*"; }
die() {
    printf '\nERROR: %s\n' "$*" >&2
    printf 'BUILD-QT-FFMPEG: FAILED\n'
    exit 1
}

check_prerequisites() {
    [[ "$(uname)" == "Darwin" ]] || die "This script is for macOS."
    [[ -x "$QT_DIR/bin/qt-configure-module" ]] || die "No Qt found at $QT_DIR (set QT_DIR=/path/to/Qt)."
    xcode-select -p >/dev/null 2>&1 || die "Install the Xcode Command Line Tools: xcode-select --install"
    for tool in cmake ninja git curl make; do
        command -v "$tool" >/dev/null 2>&1 || die "'$tool' is missing (brew install $tool)."
    done
    QT_VERSION="$("$QT_DIR/bin/qtpaths" --qt-version 2>/dev/null || "$QT_DIR/bin/qmake" -query QT_VERSION)"
    [[ -n "$QT_VERSION" ]] || die "Could not read the Qt version from $QT_DIR."
    step "Qt $QT_VERSION at $QT_DIR, FFmpeg $FFMPEG_VERSION -> $FFMPEG_PREFIX, $JOBS jobs"
}

build_ffmpeg() {
    if [[ -f "$FFMPEG_PREFIX/lib/libavcodec.dylib" ]]; then
        step "FFmpeg already built in $FFMPEG_PREFIX — skipping"
        return
    fi
    local archive="$WORK_DIR/ffmpeg-$FFMPEG_VERSION.tar.xz"
    if [[ ! -d "$WORK_DIR/ffmpeg-$FFMPEG_VERSION" ]]; then
        step "Downloading FFmpeg $FFMPEG_VERSION"
        curl -fL --retry 3 "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz" -o "$archive"
        tar -xf "$archive" -C "$WORK_DIR"
    fi
    step "Configuring FFmpeg"
    rm -rf "$WORK_DIR/ffmpeg-build"
    mkdir -p "$WORK_DIR/ffmpeg-build"
    cd "$WORK_DIR/ffmpeg-build"
    # Absolute prefix (Qt's docs: relative prefixes break the libraries' install names).
    "../ffmpeg-$FFMPEG_VERSION/configure" \
        --prefix="$FFMPEG_PREFIX" \
        --disable-doc \
        --disable-programs \
        --enable-network \
        --enable-shared
    step "Building FFmpeg (a few minutes)"
    make -j"$JOBS"
    make install
    [[ -f "$FFMPEG_PREFIX/lib/libavcodec.dylib" ]] || die "FFmpeg build finished but libavcodec.dylib is missing."
}

fetch_qtmultimedia() {
    local src="$WORK_DIR/qtmultimedia-src"
    if [[ -d "$src/.git" ]]; then
        step "Qt Multimedia sources already present — skipping download"
        return
    fi
    step "Downloading Qt Multimedia v$QT_VERSION sources"
    git clone --depth 1 --branch "v$QT_VERSION" --recurse-submodules --shallow-submodules \
        https://github.com/qt/qtmultimedia.git "$src"
}

build_qtmultimedia() {
    local build="$WORK_DIR/qtmultimedia-build"
    step "Configuring Qt Multimedia with FFmpeg"
    rm -rf "$build"
    mkdir -p "$build"
    cd "$build"
    # FFMPEG_DIR / QT_DEPLOY_FFMPEG are what configure's -ffmpeg-dir / -ffmpeg-deploy set.
    # Deploying copies the FFmpeg libraries into Qt's own lib directory.
    "$QT_DIR/bin/qt-configure-module" "$WORK_DIR/qtmultimedia-src" -- \
        -DFFMPEG_DIR="$FFMPEG_PREFIX" \
        -DQT_DEPLOY_FFMPEG=TRUE
    step "config.summary (FFmpeg lines)"
    grep -i ffmpeg config.summary || true
    grep -iE "ffmpeg[ .]*yes" config.summary >/dev/null ||
        die "Qt did not detect FFmpeg in $FFMPEG_PREFIX (see config.summary above)."

    step "Building Qt Multimedia (10–20 minutes)"
    cmake --build . --parallel "$JOBS"
}

install_qtmultimedia() {
    cd "$WORK_DIR/qtmultimedia-build"
    step "Installing into $QT_DIR"
    if [[ -w "$QT_DIR/lib" && -w "$QT_DIR/plugins/multimedia" ]]; then
        cmake --install .
    else
        echo "$QT_DIR is not writable by $(whoami): installing with sudo (type your password)."
        sudo cmake --install .
    fi
}

verify() {
    local plugin="$QT_DIR/plugins/multimedia/libffmpegmediaplugin.dylib"
    step "Verifying"
    ls -l "$QT_DIR/plugins/multimedia/"
    [[ -f "$plugin" ]] || die "$plugin was not installed."
    echo "FFmpeg libraries the plugin uses:"
    otool -L "$plugin" | grep -E "libav|libsw" || true
    printf '\nBUILD-QT-FFMPEG: SUCCESS — rebuild ScreenCam (rm -rf build, then your usual build).\n'
}

main() {
    mkdir -p "$WORK_DIR"
    # Everything also goes to the log file, so progress can be followed from elsewhere.
    exec > >(tee -a "$LOG") 2>&1
    step "build-qt-ffmpeg started"
    check_prerequisites
    build_ffmpeg
    fetch_qtmultimedia
    build_qtmultimedia
    install_qtmultimedia
    verify
}

main "$@"
