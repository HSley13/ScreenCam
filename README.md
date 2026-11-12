# ScreenCam

A cross-platform screen recorder with a **draggable camera bubble** — C++20 + Qt 6 (QML),
built with CMake. Runs on **macOS, Windows and Linux** from the same code.

**What you can do**

- Record **Screen + Camera**, **Screen only**, or **Camera only** (landscape 16:9 or portrait 9:16).
- Record the **entire screen**, **one app window**, or a **region** you draw on the screen.
  The window list shows only real app windows ("Safari — GitHub"), not the menu bar, its
  icons (clock, battery, CPU meters), the Dock, the taskbar or tool pop-ups.
- While recording, your camera floats on the desktop as a **bubble window** you can drag
  anywhere, next to a **small floating control bar** (pause / stop / timer). ScreenCam's own
  windows are left out of the recording on macOS and Windows, so the preview never shows an
  endless "screen inside the screen".
- **Edit after recording**: a Screen + Camera recording is saved as a *project*. In the editor,
  split the video into parts on a **timeline** and give each part its own bubble position,
  size and look (or hide the camera) — because the right spot depends on what's behind you.
- **Export** the edited project to a finished MP4 (with progress and cancel).
- Pick the **screen**, **camera** (built-in FaceTime HD, USB webcam, iPhone Continuity Camera…)
  and **microphone** (with a live level meter). Device lists update when you plug things in.
- Style the bubble like Loom / Screen Studio: **circle, squircle, portrait or landscape**
  frame, **size**, **corner roundness**, **border** width + colour, **drop shadow**, **mirror**.
- **Drag** the bubble anywhere on the live preview (it snaps to edges/centre) or **resize**
  it with its corner handle. You can even move it *while* recording.
- **Resolution** 720p / 1080p / 1440p / 4K / Native, **24 / 30 / 60 fps**, **countdown**
  (off / 3 / 5 / 10 s), **pause / resume**, choose the **output folder**.
- The window can **hide while recording**; control it from the **menu-bar / tray icon** or
  with **⌘⇧R / Ctrl+Shift+R** (start/stop) and **⌘⇧P / Ctrl+Shift+P** (pause).
- Output: **MP4 (H.264 + AAC)** — plays in QuickTime, Windows, browsers and phones.
  Screen-only and camera-only recordings are written straight to
  `ScreenCam 2026-11-12 at 18.42.05.mp4`; Screen + Camera recordings become
  `ScreenCam 2026-11-12 at 18.42.05.screencam` projects (see *Editing and export*).

---

## Build and install

### CMake presets

| Command | What it does |
| --- | --- |
| `cmake --preset default` / `cmake --build --preset default` | Debug build in `build/` (clangd reads `build/compile_commands.json`) |
| `ctest --preset default` | run all tests (headless) |
| `cmake --workflow --preset default` | configure + build + test in one go |
| `cmake --workflow --preset release` | Release in `build/release/`: configure + build + test + package |

Qt is found through `CMAKE_PREFIX_PATH` — export it in your shell (e.g.
`export CMAKE_PREFIX_PATH=$HOME/Qt/6.10.1/macos`) or pass `-DCMAKE_PREFIX_PATH=…` once when
configuring. Presets need CMake ≥ 3.25.

Requirements: **Qt 6.8 or newer** with **Qt Multimedia and its FFmpeg backend**, **CMake ≥ 3.24**,
**Ninja**, and a C++20 compiler.

> **The FFmpeg backend is not optional.** Screen capture (`QScreenCapture`) and feeding the
> composited frames to the encoder (`QVideoFrameInput`) only exist in Qt Multimedia's FFmpeg
> backend. Qt from the **Qt Online Installer** includes it. Homebrew's Qt does **not** (its
> `qtmultimedia` formula has no FFmpeg dependency), and a Qt you compiled yourself only has it
> if you configured it with `-ffmpeg-dir`. CMake prints a warning when the Qt it finds lacks it,
> and the app shows a banner explaining it.
> Check: your Qt's `plugins/multimedia/` must contain `libffmpegmediaplugin.*`
> (`ffmpegmediaplugin.dll` on Windows), not only `libdarwinmediaplugin.dylib`.

**Export needs the FFmpeg development files (optional).** The exporter talks to FFmpeg's
libraries directly (`libavformat`, `libavcodec`, `libavutil`, `libswscale`), so it needs their
*headers*, which Qt does not ship. CMake looks in `-DFFMPEG_DIR=<prefix>` (or the `FFMPEG_DIR`
environment variable), then `~/.local/ffmpeg-*` (where `scripts/build-qt-ffmpeg.sh` installs
it), then pkg-config and the system paths. The configure output says
`Video export: ON (FFmpeg …)` or `OFF`; without it the app still records and edits, and the
Export button explains what's missing. Prefer the **same FFmpeg major version** as Qt's
multimedia plugin (7.x for Qt 6.8–6.10), so only one copy of FFmpeg ends up in the app.

### macOS

Install Qt 6.8+ for macOS **with Qt Multimedia** using the Qt Online Installer, then:

```bash
brew install cmake ninja
cmake --preset release -DCMAKE_PREFIX_PATH=$HOME/Qt/6.10.1/macos   # your Qt version
cmake --build --preset release
open build/release/ScreenCam.app

# Install a self-contained copy (Qt and FFmpeg are copied into the bundle):
cmake --install build/release --prefix ~/Applications
```

**Using a Qt you built from source** (e.g. `/usr/local/Qt-6.10.0`): rebuild Qt Multimedia
with FFmpeg. Use the FFmpeg major version Qt documents for your Qt (7.1 for Qt 6.10):

```bash
brew install ffmpeg@7
mkdir ~/qt-build-multimedia && cd ~/qt-build-multimedia
<qt-source>/configure -prefix /usr/local/Qt-6.10.0 -submodules qtmultimedia \
    -ffmpeg-dir "$(brew --prefix ffmpeg@7)" -ffmpeg-deploy
grep -i ffmpeg config.summary        # must say "yes"
cmake --build . --parallel && cmake --install .
```

Afterwards `/usr/local/Qt-6.10.0/plugins/multimedia/` contains `libffmpegmediaplugin.dylib`.
Re-run CMake for ScreenCam (`rm -rf build && cmake --preset release`).

`scripts/build-qt-ffmpeg.sh` does all of this for you (FFmpeg into `~/.local/ffmpeg-<ver>`,
then Qt Multimedia against it) — and because that FFmpeg includes its headers, **export is
enabled automatically**: Qt and ScreenCam share the same FFmpeg. With Homebrew's FFmpeg
instead, pass `-DFFMPEG_DIR="$(brew --prefix ffmpeg@7)"`. On macOS the export uses the
hardware H.264 encoder (VideoToolbox), so it's fast and FFmpeg needs no extra codecs.

**Permissions (do this once).** macOS remembers Camera / Microphone / Screen Recording
permission per *code signature*. An ad-hoc signature changes with every build, so macOS would
forget the permission after each rebuild. Create a fixed signing identity once:

```bash
scripts/create-macos-signing-identity.sh       # asks for your password to trust it
rm -rf build && cmake --preset default && cmake --build --preset default   # now signed with it
tccutil reset ScreenCapture com.hsley13.screencam                          # drop stale entries
open build/ScreenCam.app
```

Allow **Camera** and **Microphone** when asked; enable **Screen Recording** in *System Settings ▸
Privacy & Security ▸ Screen & System Audio Recording*, then **quit (⌘Q) and reopen** ScreenCam —
macOS only re-checks screen access when the app starts. Start it with `open` (or from Finder):
launched from a terminal, macOS checks the *terminal's* permission instead of ScreenCam's.

### Windows

Install Visual Studio 2022 (C++ workload) and Qt 6.8+ for *MSVC 2022 64-bit* with
*Qt Multimedia* (Qt Online Installer). In an **x64 Native Tools Command Prompt**:

For export, download an FFmpeg 7 **shared** build that includes `include/` and `lib/`
(e.g. the "full_build-shared" archive from gyan.dev or a BtbN "shared" build), unpack it to
`C:/ffmpeg` and add `-DFFMPEG_DIR=C:/ffmpeg`. Exports use the Media Foundation H.264 encoder.

```bat
cmake --preset release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 -DFFMPEG_DIR=C:/ffmpeg
cmake --build --preset release
cmake --install build/release --prefix "%LOCALAPPDATA%/Programs/ScreenCam"
```

### Linux

Use Qt 6.8+ from the Qt Online Installer (includes FFmpeg). Distro packages work only if they
ship the FFmpeg multimedia plugin (`libffmpegmediaplugin.so`); Ubuntu 24.04's Qt 6.4 is too old.

For export: `sudo apt install libavformat-dev libavcodec-dev libswscale-dev` (Debian/Ubuntu)
or your distribution's `ffmpeg-devel`. The export uses libx264 when that FFmpeg has it.

```bash
cmake --preset release -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build --preset release
cmake --install build/release --prefix ~/.local     # adds a launcher + icon
```

Screen capture on Linux goes through Qt's FFmpeg backend; on a **Wayland** session it may not
be able to capture the screen with Qt 6.8 — use an X11 session if the preview stays black.
Linux has no way for an app to hide its own windows from a screen capture, so there the
floating bubble and control bar are visible in full-screen recordings (record a window or a
region next to them to avoid that).

---

## Editing and export

**Why a project instead of a video.** A finished video has the bubble burnt into it — to
move it afterwards you'd have to record again. So a Screen + Camera recording keeps the two
tracks separate:

```
ScreenCam 2026-11-12 at 18.42.05.screencam/
├── screen.mp4     the screen / window / region, with your microphone
├── camera.mp4     your camera, full frame, unmirrored, no audio
└── project.json   the timeline: where the bubble is, and how it looks, from when to when
```

Both files are recorded at the same time by two `RecordingPipeline`s. Their clocks start a few
milliseconds apart; that difference is stored as `cameraOffsetMs` (camera time = video time −
offset) so the editor and the exporter line them up. If lips and voice still look off, the
editor's *Camera sync* slider nudges it.

**The editor** opens by itself when a recording ends (or *Open project…* / *Edit recording*,
or `ScreenCam --open My.screencam`):

- The timeline is a list of **parts** (segments). Each starts where you split and lasts until
  the next split. Press **S** (or *Split here*) where the background changes, then drag
  yourself in the preview — that moves the camera *for this part only*. A new part starts with
  the look of the one it was cut from.
- The right panel edits the part under the playhead: shape, size, roundness, border, shadow,
  mirror, quick corner positions, *Show camera in this part*, *Use this look for all parts*.
- **Delete** removes a part; its neighbour takes over its time. Changes save automatically.
- Keys: **Space** play/pause · **S** split · **⌫** delete part · **← →** ±1 s · **Home** start.

**Export** writes `<project name>.mp4` next to the project folder (exporting again replaces
it). It decodes both files, paints every frame with the *same* `FrameCompositor` the recorder
uses (so the exported bubble is pixel-for-pixel what you placed), encodes H.264
(VideoToolbox on macOS, Media Foundation on Windows, libx264 / OpenH264 where available, MPEG-4
as a last resort) and copies the microphone audio as-is. It runs in the background, faster
than real time on most machines; a canceled export leaves no file behind.

### Packages and CI

`cmake --build build/release --target package` creates a `.dmg` (macOS), `.zip` (Windows) or
`.tar.gz` (Linux). After you push to GitHub, **Actions** builds, tests and packages all three
platforms on every push to `main` — download them from the run's *Artifacts*.

### Tests

```bash
cmake --workflow --preset default        # configure + build + test (Debug, in build/)
```

| Test | What it proves |
| --- | --- |
| `core_tests` | sizes, bubble geometry, snapping, crop, NV12 colours, meter maths |
| `tst_compositor` | pixels: circle mask, border, mirror, crop, NV12 format, pause-aware timestamps |
| `tst_pipeline` | a real MP4 is written: duration excludes the pause, 640×360, decoded colours correct |
| `tst_project` | timeline rules (split, delete, order) and `project.json` round trip |
| `tst_media_backend` | detection of a Qt without the FFmpeg multimedia backend |
| `make_sample_project` | (fixture) records a 3-second synthetic project for the tests below |
| `tst_project_editor` | per-part styles, selection isn't an edit, autosave, export path |
| `tst_export` | exported MP4 has the bubble where each part puts it, hidden camera, cancel, worker thread |
| `tst_app_windows` | (Linux, Xvfb) a fake desktop: only the app window is listed, and Qt's entry carries the same id |
| `ui_smoke` | the whole QML UI loads headless with **zero** QML warnings (`ScreenCam --self-test`) |
| `ui_smoke_editor` | the editor loads the sample project with zero QML warnings |

`ScreenCam --self-test --screenshot shot.png` also saves a picture of the window, and
`make_sample_project ~/Desktop/Test.screencam 5` gives you a project to try the editor with.

clangd (Neovim) picks up `build/compile_commands.json` from the `default` preset; `.clang-format` matches the style.

---

## How it works

### The big picture

While recording (Screen-only / Camera-only write one composited MP4 like this; Screen + Camera
runs two of these pipelines — screen-only and camera-only — into a project):

```
 QScreenCapture ─► QVideoSink ─┬─► QML preview (VideoOutput)            CaptureSources
 QCamera        ─► QVideoSink ─┤                                        (main thread)
                               └─► latest frames ─────────────┐
                                                              ▼
   BubbleStyle (shape, size, position…) ─────────► FrameCompositor     (own thread)
                                                   paint canvas → NV12 → timestamp
                                                              │
 QAudioInput (mic) ──► QMediaCaptureSession ◄── QVideoFrameInput ◄──┘   RecordingPipeline
                              │
                              ▼
                       QMediaRecorder  →  MP4 (H.264 + AAC)
```

### Why a floating bubble *and* separate tracks

The camera bubble you see while recording is a real always-on-top window, so you can see
yourself and drag it out of the way of what you're showing. But it is **not** recorded as
part of the screen: ScreenCam's windows are excluded from capture (`NSWindowSharingNone` on
macOS, `WDA_EXCLUDEFROMCAPTURE` on Windows — `WindowTools`), and the camera is recorded on its
own track. Its position when you stopped becomes the first part's position in the editor;
the final picture is composited at export time. That gives you:

- no "screen inside the screen" loop in the preview,
- a bubble you can still move, resize and restyle per part after recording,
- one compositor (`FrameCompositor`) for live recordings and exports alike.

### Two experiments that shaped the encoder path

1. **Timestamps.** Frames pushed through `QVideoFrameInput` *without* timestamps are timed
   by frame count: 30 frames sent in 0.3 s came out as a 1.0 s video. With explicit
   `startTime`/`endTime` they came out at exactly 3.0 s. Real frames never arrive perfectly
   evenly, so every frame is stamped with the real elapsed time — otherwise the video drifts
   away from the microphone audio.
2. **Pixel format.** Sending RGB frames made x264 choose **4:4:4**, which QuickTime refuses
   to play. Converting every frame to **NV12 (4:2:0, BT.709)** gives a standard
   *High profile, yuv420p* stream that plays everywhere.

### Layers

```
src/core/    pure C++20, no Qt — maths only, unit-tested anywhere
src/media/   Qt Gui + Multimedia, no QML — compositing and encoding (tested headless)
src/export/  FFmpeg (libav*) — decode + composite + encode a project (optional)
src/app/     QML-facing objects: settings, devices, capture, recorder, editor
qml/         the UI (Basic style + Theme.qml, identical on every OS)
```

Dependencies point downwards only: `qml → app → export → media → core`.

---

## Walkthrough, file by file

### `src/core/` — the maths (no Qt)

- **`Resolution`** — `computeCaptureSize(display, preset)`: the preset fixes the *height*, the
  width scales by the same factor (a 16:10 screen at "720p" → 1108×720, not a stretched
  1280×720); never upscales; `makeEven()` because 4:2:0 video needs even dimensions.
- **`CameraLayout`** — `cameraRecordingSize(camera, orientation)`: largest centred 16:9 or
  9:16 rectangle inside the camera image (1080p webcam → portrait 608×1080).
- **`BubbleGeometry`** — the single source of truth for the bubble:
  - `bubbleSize()` — size as a fraction of the canvas's *shorter* side, so the bubble looks
    the same on landscape and portrait videos; rectangles get a 25% longer long side.
  - `bubbleRect()` / `clampNormalizedCenter()` — position is a **normalized centre (0..1)**,
    so it means the same in a 900-px preview and a 4K file; clamped 3% inside the edges.
  - `snapNormalizedCenter()` — on release, each axis jumps to start/middle/end if close.
  - `bubbleCornerRadius()` — circle = half the side; other shapes use *roundness*.
  - `aspectFillCrop()` — the centred part of the camera image that fills a shape without
    stretching.
- **`Nv12`** — `convertXrgbToNv12()`: full-resolution Y plane + one (U,V) pair per 2×2 block
  (averaged, so coloured text stays clean). Coefficients are BT.709 in 8.8 fixed point; each
  chroma row is rounded to sum to **exactly 0**, so greys get no colour tint (a test caught
  the off-by-one when it didn't).
- **`EncoderPlan`** — `planEncoder()`: bit rate = pixels/s × bits-per-pixel, clamped 2–60 Mbit/s.
- **`AudioLevel`** — `peakToMeterLevel()` maps a sample peak to a dB-scaled 0..1 bar
  (-60 dBFS → 0, 0 dBFS → 1); `smoothMeterLevel()` makes it rise instantly and fall slowly.
- **`TimeFormat`** — `formatElapsed()` → `00:42`, `1:02:03`.
- **`Timeline`** — `segmentAt()`, `canSplitAt()` (cuts at least 0.5 s apart, not at the
  ends), `insertionIndex()`: the rules every timeline edit goes through.
- **`WindowFilter`** — `isUserAppWindow()`: what counts as an app window (normal window level,
  owned by a regular app, not a tool/dock/pop-up window, not hidden on another desktop, has
  minimize/maximize, at least 120×80); `windowLabel()` → `"Safari — GitHub"`.
- **`BubbleGeometry::aspectFitRect()`** — letterboxing a window/region into the canvas.

### `src/media/` — painting and encoding

- **`FrameCompositor`** (runs on its own thread)
  - `setScreenFrame()` / `setCameraFrame()` store the latest frames; `refreshImages()`
    converts them to images **only when a tick needs them** (a 60 Hz screen and 30 fps camera
    would otherwise cost conversions that are thrown away).
  - `renderCanvas()` — paints one frame: screen scaled to the canvas, then `drawBubble()`.
  - `drawBubble()` — (1) a cached, box-blurred **shadow** (`shadowFor()`); (2) the camera,
    cropped by `aspectFillCrop()` and optionally mirrored, masked by a cached **anti-aliased
    mask image** (`maskFor()`) with `DestinationIn` — a mask *image*, because composition
    modes only affect pixels a fill touches, so a path alone would leave the corners;
    (3) the **border**, drawn inside the shape like a QML border.
  - `toNv12Frame()` — canvas → NV12 `QVideoFrame` tagged BT.709 / video range.
  - `tick()` / `activeMicroseconds()` — a `PreciseTimer` drives the frame rate; each frame is
    stamped with elapsed time **minus paused time**, so pauses never appear in the video.
- **`RecordingPipeline`** — wires `QVideoFrameInput` + optional `QAudioInput` into a
  `QMediaCaptureSession` with a `QMediaRecorder` (MP4/H.264/AAC).
  - `start()` configures the recorder and compositor (blocking call so the first frame is
    right), starts recording, then starts the compositor clock.
  - `sendFrame()` drops a frame when the encoder queue is full instead of blocking.
  - `pause()`/`resume()` pause both the recorder (audio) and the compositor clock (video).
  - `stop()` waits for the compositor to stop, then `finish()` reports `finished(path)` or
    `failed(message)`.
- **`BubbleStyleSnapshot`** — a plain copy of the bubble style that is safe to hand to the
  compositor thread; `borderPixels()` scales the border with the canvas.
- **`Project`** — the `.screencam` folder in memory: files, `cameraOffsetMs`, canvas size and
  the `segments` (start time + bubble style + camera visible). `splitAt()` copies the current
  part's look into the new one; `removeSegment()` gives the time to a neighbour;
  `save()`/`load()` read and write `project.json` (and repair the order of hand-edited files).

### `src/export/` — turning a project into a video (FFmpeg)

- **`FFmpegUtil.hpp`** — `std::unique_ptr` owners for FFmpeg's C structs (every early
  `return` frees them) and `errorText()` for readable errors.
- **`VideoFileReader`** — decodes one file front to back. `imageAt(t)` returns the frame a
  player would show at `t` (the latest one with timestamp ≤ t), so the exporter can step at a
  steady 30 fps while the source frames came unevenly. Converts with `swscale`, honouring the
  file's BT.601/709 tag and range.
- **`ProjectExporter`** — `exportProject()` is the whole export, synchronously (tests call it
  directly); `start()` runs it on a low-priority `QThread` and reports `progress`, `finished`,
  `failed`, `canceled`.
  - `openEncoder()` tries encoders best-first and keeps the first that opens.
  - per frame: segment at `t` → `FrameCompositor::setBubble()`, screen at `t`, camera at
    `t − cameraOffsetMs` → `renderCanvas()` → `fillFrame()` (NV12, or split into YUV420P).
  - `AudioCopy::writeUntil()` copies AAC packets up to the current video time, so audio and
    video stay interleaved in the file.
  - `Muxer` writes to `<name>.mp4.part` and renames it only when everything succeeded.

### `src/app/` — what QML talks to

- **`DeviceCatalog`** (`Devices`) — cameras/microphones/screens as `{id, name}` lists (with a
  "None" entry), refreshed on hot-plug; `requestPermissions()` (camera, then mic);
  `findCamera()/findMicrophone()/findScreen()` turn stored ids back into devices;
  `screenPixelSize()` = logical size × device pixel ratio.
- **`RecordingSettings`** — every sidebar choice, saved with `QSettings` through one
  `assign()` template (skip no-ops, persist, notify).
- **`BubbleStyle`** — the bubble's look/position, persisted; `rectFor()`, `radiusFor()`,
  `borderFor()`, `moveCenter()`, `snap()`, `placeInCorner()` are thin wrappers over
  `core/BubbleGeometry` so QML and the compositor compute identical geometry. `revision`
  lets QML bindings that call these functions re-evaluate on any change.
- **`CaptureSources`** (`Capture`) — runs the screen capture and camera only while the
  preview is visible or a recording runs (camera light off otherwise); fans each frame out to
  QML previews and to the recorder; `pickCameraFormat()` chooses ≤1080p at ≥24 fps;
  `outputSize()` asks the core for the final video size.
- **`AudioLevelMeter`** — a `QAudioSource` on the selected mic; peak per chunk via
  `QAudioFormat::normalizedSampleValue()` (works for any sample format).
- **`RecordingController`** (`Recorder`) — state machine
  `Idle → Countdown → Recording ⇄ Paused → Finalizing → Idle`. `start()` validates and
  keeps inputs alive; `beginRecording()` hides the window (with a short delay so the minimize
  animation isn't recorded), snapshots settings into a pipeline request and starts it;
  bubble edits during recording are forwarded live; `nextOutputPath()` names files.
- **`ProjectEditor`** (QML type, one per open project) — the editor's model. The part under
  the playhead is exposed as `style`, an ordinary (non-persistent) `BubbleStyle`, so the same
  bubble UI that edits the recording defaults edits a part; `onStyleEdited()` copies changes
  back (and `loadCurrentStyle()` uses a flag so *selecting* a part isn't mistaken for an
  edit). Autosaves 0.6 s after the last change. `exportVideo()` / `cancelExport()` /
  `playLastExport()` drive the exporter when it was built.
- **`WindowTools`** — `excludeFromCapture(window)` (macOS in `WindowTools_mac.mm`).
- **`AppWindows`** — `listAppWindows()` asks the OS about every open window and keeps the
  ones `WindowFilter` accepts. Qt's own list (`QWindowCapture::capturableWindows()`) contains
  *every* window with only a title, so `CaptureSources::refreshWindows()` keeps the Qt
  entries whose native id (read from Qt's private `QCapturableWindowPrivate`) is in ours.
  - macOS (`AppWindows_mac.mm`): `CGWindowListCopyWindowInfo` gives the level, size and
    owner; `NSRunningApplication.activationPolicy` tells a Dock app from a menu-bar utility.
  - Windows (`AppWindows_win.cpp`): `EnumWindows` + the taskbar's rules (`WS_EX_TOOLWINDOW`,
    owner window, `WS_EX_APPWINDOW`), `DWMWA_CLOAKED` for other virtual desktops, the app
    name from the .exe's version info.
  - Linux (`AppWindows_x11.cpp`): `_NET_CLIENT_LIST` and the EWMH window type/state/allowed
    actions; app name from `WM_CLASS`.
- **`main.cpp`** — `QApplication` (the tray icon needs Widgets on Windows/Linux), Basic
  style, window icon, `--open <project>`, and `--self-test [--screenshot file]` (load the UI,
  exit non-zero on QML warnings).

### `qml/`

- **`Main.qml`** — layout, tray icon (`Qt.labs.platform`), shortcuts, hide/show handling.
- **`PreviewStage.qml`** — letter-boxed canvas with the output's aspect ratio, live screen,
  camera-only view, snap guides while dragging.
- **`CameraBubble.qml`** — `VideoOutput` masked with `MultiEffect`, shadow, border, drag with
  `DragHandler` (normalized deltas → `BubbleStyle.moveCenter`), resize handle placed where the
  shape's edge crosses the diagonal (`radius × (1 − 1/√2)` in from the corner).
- **`SettingsPanel.qml`, `BubbleSection.qml`** — the sidebar. **`ControlBar.qml`** — record
  button, timer (tabular digits so it doesn't jiggle), pause/stop, play last recording.
- **`FloatingBubble.qml`, `MiniControlBar.qml`, `RegionSelector.qml`** — the desktop bubble
  window, the floating recording controls, and the full-screen rubber band for regions.
- **`EditorPage.qml`** — two `MediaPlayer`s: screen (with audio) is the master clock; the
  camera follows at `position − cameraOffsetMs`, re-synced on every seek/play/pause and when
  it drifts more than 120 ms. The bubble is the same `CameraBubble` as the recorder, fed by
  the camera player. **`EditorTimeline.qml`** — parts, playhead, ticks, click/drag to scrub.
  **`EditorInspector.qml`** — the part's settings (reuses `BubbleSection`).
- **`components/`** — themed controls. `StyledComboBox` re-binds `currentIndex` after a pick,
  because a ComboBox assigns its own index on click and would otherwise break the binding to
  the setting.

---

## Troubleshooting

- **Empty screen preview + banner "built without the FFmpeg multimedia backend"** — your Qt
  has only the native backend (e.g. `plugins/multimedia/libdarwinmediaplugin.dylib`). Install
  Qt with the Qt Online Installer, or rebuild Qt Multimedia with `-ffmpeg-dir` (see *macOS*).
- **"macOS hasn't given ScreenCam access to the screen" even though you allowed it** — the
  build you're running has a different signature than the one you allowed (each ad-hoc build
  does), or it was launched from a terminal, or it wasn't restarted after allowing. Follow
  *Permissions (do this once)* above.
- **"The screen capture is running but delivers no picture"** — same causes as above.
- **Log lines `Packet with invalid duration … in stream 0`** — printed by FFmpeg's MP4 muxer
  for B-frames that Qt 6.8 doesn't give a duration. Harmless: `tst_pipeline` checks that
  frame times increase and the file plays with the right duration.
- **"This build of ScreenCam can't export"** — CMake didn't find the FFmpeg development
  files; see *Export needs the FFmpeg development files* and re-configure with `-DFFMPEG_DIR`.
- **Exported video's camera is slightly out of sync with your voice** — use *Camera sync* in
  the editor's *Project* section, then export again.
- **High CPU at 4K / 60 fps** — painting and colour conversion run on the CPU; 1080p/30 is
  light, 4K/60 is demanding on older machines.

## History

Every feature was developed on its own branch and merged into `main` with `--no-ff`. The first,
macOS-only Objective-C++ version is kept under the tag `archive/macos-native`.
