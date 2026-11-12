# ScreenCam

A cross-platform screen recorder with a **draggable camera bubble** — C++20 + Qt 6 (QML),
built with CMake. Runs on **macOS, Windows and Linux** from the same code.

**What you can do**

- Record **Screen + Camera**, **Screen only**, or **Camera only** (landscape 16:9 or portrait 9:16).
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
  Files are named `ScreenCam 2026-11-12 at 18.42.05.mp4`.

---

## Build and install

Requirements: **Qt 6.8 or newer** with the **Qt Multimedia** module, **CMake ≥ 3.24**,
**Ninja**, and a C++20 compiler.

### macOS

```bash
brew install qt cmake ninja
cmake --preset release
cmake --build --preset release
open build/release/ScreenCam.app

# Install a self-contained copy (Qt is copied into the bundle):
cmake --install build/release --prefix ~/Applications
```

Permissions: macOS asks for **Camera** and **Microphone** the first time. **Screen Recording**
must be enabled in *System Settings ▸ Privacy & Security ▸ Screen & System Audio Recording*;
then quit and reopen ScreenCam. The build is ad-hoc signed, so after reinstalling a new build
macOS may ask again (remove and re-add ScreenCam in that list if the preview stays black).

### Windows

Install Visual Studio 2022 (C++ workload) and Qt 6.8+ for *MSVC 2022 64-bit* with
*Qt Multimedia* (Qt Online Installer). In an **x64 Native Tools Command Prompt**:

```bat
cmake --preset release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build --preset release
cmake --install build/release --prefix "%LOCALAPPDATA%/Programs/ScreenCam"
```

### Linux

Use Qt 6.8+ (Qt Online Installer, or a distro that ships it, e.g. Arch or recent Fedora;
Ubuntu 24.04's Qt 6.4 is too old).

```bash
cmake --preset release -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build --preset release
cmake --install build/release --prefix ~/.local     # adds a launcher + icon
```

Screen capture on Linux goes through Qt's FFmpeg backend; on a **Wayland** session it may not
be able to capture the screen with Qt 6.8 — use an X11 session if the preview stays black.

### Packages and CI

`cmake --build build/release --target package` creates a `.dmg` (macOS), `.zip` (Windows) or
`.tar.gz` (Linux). After you push to GitHub, **Actions** builds, tests and packages all three
platforms on every push to `main` — download them from the run's *Artifacts*.

### Tests

```bash
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```

| Test | What it proves |
| --- | --- |
| `core_tests` | sizes, bubble geometry, snapping, crop, NV12 colours, meter maths |
| `tst_compositor` | pixels: circle mask, border, mirror, crop, NV12 format, pause-aware timestamps |
| `tst_pipeline` | a real MP4 is written: duration excludes the pause, 640×360, decoded colours correct |
| `ui_smoke` | the whole QML UI loads headless with **zero** QML warnings (`ScreenCam --self-test`) |

clangd (Neovim) reads `build/<preset>/compile_commands.json`; `.clang-format` matches the style.

---

## How it works

### The big picture

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

### Why the bubble is *composited*, not a floating window

The macOS-only first version (tag `archive/macos-native`) put the camera in a real
always-on-top window and simply recorded the screen. That doesn't port: hiding the control
window from the capture is OS-specific, and Wayland doesn't even let apps position windows.
So now the screen and camera are captured **separately** and painted together:

- the preview in the app shows exactly the final framing (same geometry code, see below),
- you can restyle or drag the bubble mid-recording and the video follows,
- Camera-only mode is just another layout of the same compositor.

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
src/app/     QML-facing singletons: settings, devices, capture, recorder
qml/         the UI (Basic style + Theme.qml, identical on every OS)
```

Dependencies point downwards only: `qml → app → media → core`.

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

### `src/app/` — what QML talks to (all singletons)

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
- **`main.cpp`** — `QApplication` (the tray icon needs Widgets on Windows/Linux), Basic
  style, window icon, and `--self-test` (load the UI, exit non-zero on QML warnings).

### `qml/`

- **`Main.qml`** — layout, tray icon (`Qt.labs.platform`), shortcuts, hide/show handling.
- **`PreviewStage.qml`** — letter-boxed canvas with the output's aspect ratio, live screen,
  camera-only view, snap guides while dragging.
- **`CameraBubble.qml`** — `VideoOutput` masked with `MultiEffect`, shadow, border, drag with
  `DragHandler` (normalized deltas → `BubbleStyle.moveCenter`), resize handle placed where the
  shape's edge crosses the diagonal (`radius × (1 − 1/√2)` in from the corner).
- **`SettingsPanel.qml`, `BubbleSection.qml`** — the sidebar. **`ControlBar.qml`** — record
  button, timer (tabular digits so it doesn't jiggle), pause/stop, play last recording.
- **`components/`** — themed controls. `StyledComboBox` re-binds `currentIndex` after a pick,
  because a ComboBox assigns its own index on click and would otherwise break the binding to
  the setting.

---

## Troubleshooting

- **Black screen preview on macOS** — grant Screen Recording (see above) and reopen the app.
- **Log lines `Packet with invalid duration … in stream 0`** — printed by FFmpeg's MP4 muxer
  for B-frames that Qt 6.8 doesn't give a duration. Harmless: `tst_pipeline` checks that
  frame times increase and the file plays with the right duration.
- **High CPU at 4K / 60 fps** — painting and colour conversion run on the CPU; 1080p/30 is
  light, 4K/60 is demanding on older machines.

## History

Every feature was developed on its own branch and merged into `main` with `--no-ff`. The first,
macOS-only Objective-C++ version is kept under the tag `archive/macos-native`.
