# ScreenCam

A small macOS app that records your screen together with a **draggable, circular camera
bubble** — written in C++20 / Objective-C++ and built with CMake.

- Pick the **screen**, the **resolution** (720p · 1080p · 1440p · 4K · Native Retina),
  the **camera** (built-in FaceTime HD, USB webcam, iPhone Continuity Camera) and the
  **microphone**.
- Your camera shows in a **circle** you can drag anywhere — it is recorded exactly where you
  put it. Right-click the bubble for Small / Medium / Large.
- **Camera mode — Landscape or Portrait** — decides the framing of a second file that
  contains only you (16:9 or 9:16, cut from the centre of the camera image).
- The settings window is **hidden from the recording**, so it can stay open while you record.

Every recording produces (in `~/Movies/ScreenCam/`):

| File | Contents |
| --- | --- |
| `ScreenCam <date> - Screen.mov` | screen at the chosen resolution + the bubble + mic audio |
| `ScreenCam <date> - Camera (Portrait).mov` | only the camera, 9:16 or 16:9, + mic audio (only when a camera is selected) |

Requires **macOS 13 Ventura or newer** (ScreenCaptureKit) and the Xcode Command Line Tools.

---

## Build, run, test

```bash
cmake -S . -B build -G Ninja          # or omit -G Ninja to use Makefiles
cmake --build build
open build/ScreenCam.app              # use `open`, see "Permissions" below

ctest --test-dir build --output-on-failure   # unit tests for the portable C++ core
```

`build/compile_commands.json` is generated, so **clangd in Neovim** understands the
Objective-C++ files too (point clangd at `build/` or symlink the file to the repo root).

### Permissions (read this once)

macOS asks for three permissions: **Camera**, **Microphone** and **Screen Recording**.

1. Launch with `open build/ScreenCam.app`. If you start the binary inside the bundle from
   your terminal, macOS attributes the permission to *Ghostty/Terminal* instead of ScreenCam.
2. Screen Recording can only be granted in **System Settings ▸ Privacy & Security ▸ Screen &
   System Audio Recording**. After enabling it, **quit and reopen** ScreenCam.
3. The build is *ad-hoc signed* (CMake runs `codesign --sign -` after linking). macOS keys
   permissions to that signature, so after a rebuild it may ask again. If Screen Recording
   seems "on" but doesn't work, remove ScreenCam from that list with `–` and add it again.

---

## How it works

### The key idea: the bubble is a real window

There are two ways to put a circle of camera video into a screen recording:

1. Capture screen and camera separately and **draw** the circle into every screen frame.
2. Show the camera in an actual on-screen window shaped like a circle, and simply **record the
   screen**.

ScreenCam does (2). Whatever you see is what gets recorded — dragging the bubble *is* moving
it in the video, with zero per-frame compositing code. The only window we must keep out of the
recording is the control panel, and ScreenCaptureKit can exclude specific windows.

### Data flow while recording

```
 ScreenCaptureKit ──BGRA frames (host clock)──────────┐
                                                       ├─▶ RecordingSession ─▶ MovieWriter "Screen.mov"
 Mic (AVCaptureSession) ──48 kHz PCM──retime──────────┤        │   (audio fanned out to both)
                                                       │        ▼
 Camera (AVCaptureSession) ──NV12 frames──retime──────┴─▶ MovieWriter "Camera (…).mov"
          │
          └──▶ AVCaptureVideoPreviewLayer ─▶ CameraBubbleWindow (mirrored, always visible)
```

### Layers

```
src/core/       pure C++20, no Apple headers — all the "math", unit-tested with CTest
src/platform/   Objective-C++ wrappers over AVFoundation / ScreenCaptureKit / CoreMedia
src/recording/  encoding to .mov and orchestration of one recording
src/ui/         AppKit windows (control panel, camera bubble)
src/app/        entry point + AppDelegate that wires everything together
```

Dependencies only point downwards (`app → ui/recording → platform → core`), which is why
`core` builds and tests on any OS.

---

## Walkthrough, file by file

### `src/core/` — the portable C++ core

**`Resolution.hpp/.cpp`**
- `PixelSize` — width/height in *physical* pixels. macOS APIs mostly speak in *points*
  (on Retina, 1 point = 2×2 pixels); mixing the two is the classic bug this type avoids.
- `resolutionOptions()` — the presets shown in the UI, as one table (label + target height),
  so UI and logic can't disagree.
- `makeEven(v)` — H.264/HEVC store colour at half resolution (4:2:0), so both dimensions must
  be even. Rounds down, minimum 2.
- `computeCaptureSize(display, preset)` — the screen file's size. The preset fixes the
  *height*; the width is scaled by the same factor, so a 16:10 MacBook screen at "720p"
  becomes 1108×720 rather than a stretched 1280×720. It never upscales (4K on a 1080p
  display stays 1080p) and always returns even numbers.

**`CameraLayout.hpp/.cpp`**
- `cameraRecordingSize(camera, orientation)` — the largest centred 16:9 (landscape) or 9:16
  (portrait) rectangle inside the camera frame. Try full height first; if the result is wider
  than the camera (e.g. landscape from a 4:3 camera), use full width instead.
  Example: 1920×1080 webcam → portrait = 1080·9/16 = 607.5 → **608×1080**.

**`EncoderPlan.hpp/.cpp`**
- `requiresHevc(size)` — Apple's H.264 encoder tops out around 4096×2304; "Native" on a 5K
  display needs HEVC. Compares long/short sides so portrait sizes work too.
- `planEncoder(size, fps)` — bit rate = pixels per second × bits-per-pixel (0.1 for H.264,
  0.07 for HEVC), clamped to 2–60 Mbit/s; key frame every 2 s for snappy seeking.
  1080p30 → 1920·1080·30·0.1 ≈ 6.2 Mbit/s.

**`TimeFormat.hpp/.cpp`**
- `formatElapsed(seconds)` — `00:42`, `12:03`, `1:02:03` for the status label.

**`tests/core_tests.cpp`** — a tiny `CHECK` macro and one test function per module; run by
CTest. Every number in the examples above is asserted there.

### `src/platform/` — talking to the OS

**`DeviceCatalog`** — read-only hardware queries.
- `cameras` / `microphones` — `AVCaptureDeviceDiscoverySession` with the right device types
  (on macOS 14+: built-in, external and Continuity Camera; on 13: the older types). Results
  are de-duplicated by `uniqueID` because one device can match two types.
- `displays` + `pixelSizeForDisplay:` — `NSScreen` for names, `CGDisplayCopyDisplayMode` for
  the *real* pixel count.
- `screenForDisplayID:` / `displayIDForScreen:` — translate between AppKit's `NSScreen` and
  CoreGraphics' `CGDirectDisplayID` (needed to put the bubble on the recorded screen).

**`CameraCapture`** — one long-lived camera session.
- `switchToDeviceWithUniqueID:completion:` — swaps the input inside
  `beginConfiguration`/`commitConfiguration` on a background queue (because `startRunning`
  blocks), picks 1080p → 720p → best preset, then starts/stops the session.
- `applyMirroring` — mirrors only the *preview* (people expect a mirror), not recorded frames.
- `frameSink` — while recording, `RecordingSession` is attached here and receives every frame;
  otherwise frames are dropped and only the preview runs.

**`MicCapture`** — one microphone, created per recording so the mic indicator is only on
while you record. `audioSettings` (a macOS-only feature) forces every mic to deliver 48 kHz
mono 16-bit PCM, so the AAC encoder settings always match.

**`ScreenCapture`** — wraps `SCStream`.
- Step 1 `SCShareableContent` lists displays/windows (and triggers the permission check).
- Step 2 builds an `SCContentFilter` for the display **excluding the control panel window**,
  and an `SCStreamConfiguration` with the size from `computeCaptureSize`, 30 fps, BGRA, cursor.
- `IsCompleteFrame` — ScreenCaptureKit only sends new pixels when the screen changes and also
  delivers "idle" buffers without an image; only `Complete` frames are passed on.

**`ClockSync`** — `SCCopySampleBufferOnHostClock` converts a buffer's timestamps from a
capture session's clock to the host clock (the clock ScreenCaptureKit uses). AVAssetWriter
places samples on the timeline *only* by timestamp, so all sources must share one clock or
the audio would drift. It's a shallow copy — the pixels/audio are not duplicated.

**`Bridging.h` / `Errors.h`** — `SCString()` (C++ string → `NSString`) and `SCMakeError()`.

### `src/recording/`

**`MovieWriter`** — one `.mov` = one `AVAssetWriter` with an H.264/HEVC video input and an
optional AAC input.
- **Crop for free:** `AVVideoScalingModeResizeAspectFill` makes the encoder scale incoming
  frames to *fill* the output size and crop the overflow from the centre. That is how the
  608×1080 portrait file is cut from a 1920×1080 webcam without touching a pixel ourselves.
- `appendVideoSampleBuffer:` — the first video frame calls `startSessionAtSourceTime:` and
  becomes t = 0; later frames must have strictly increasing timestamps; if the encoder is
  busy the frame is dropped rather than stalling the camera.
- `appendAudioSampleBuffer:` — ignores audio from before the first video frame.
- `finishAtSourceTime:` — ends the movie at the *moment Stop was pressed*, not at the last
  frame. A static screen produces few frames; without this the video would end early.
- A lock (`@synchronized`) serializes appends, because screen, camera and mic call in from
  three different queues.

**`RecordingSession`** — one recording from Start to Stop.
- `prepareWritersWithError:` — creates `~/Movies/ScreenCam`, asks the core for both sizes,
  creates the writers.
- `startWithCompletion:` — mic first (it takes a moment), then attaches to the camera, then
  starts the screen stream. Any failure tears everything down again.
- Sinks — screen frames go straight to the screen writer; camera frames are retimed, then go
  to the camera writer; mic audio is retimed and **fanned out to both** writers.
- `stopWithCompletion:` — records the stop time, detaches camera + mic, stops the stream,
  finalizes all writers in parallel (`dispatch_group`) and reports the saved files.

### `src/ui/`

**`CameraBubbleWindow`** — an `NSPanel` that is borderless + **non-activating** (dragging it
doesn't steal focus from the app you're presenting), transparent, floating above all apps and
on every Space. Its content view is *layer-hosting*: a `CALayer` with
`cornerRadius = diameter / 2` and `masksToBounds` turns the square preview into a circle, and
its border draws the white ring. `mouseDown:` hands the event to
`performWindowDragWithEvent:` so the window server moves it smoothly. `placeOnScreen:` puts it
in the bottom-left of the recorded display unless it's already there.

**`ControlPanelController`** — the settings window built in code with `NSGridView`. Device
ids are stored in each menu item's `representedObject` (`nil` = "None").
`reloadSources` keeps your selection across hot-plugs. It only *reports* changes through
`ControlPanelDelegate`; it never touches capture objects itself.

### `src/app/`

**`main.mm`** — creates `NSApplication`, sets the delegate, runs the event loop.

**`AppDelegate`** — the glue.
- Launch: builds the menu, creates camera + bubble + panel, requests permissions.
- `applySelection` — makes the camera session and the bubble match the panel.
- `startRecording` — snapshots the panel into `RecordingOptions` (excluding the panel window)
  and starts a `RecordingSession`; a 0.5 s timer updates `● Recording 00:42`.
- `applicationShouldTerminate:` — quitting mid-recording first finishes the files
  (`NSTerminateLater`), because a `.mov` is unreadable until its index is written at the end.

---

## Branches

Each feature was developed on its own branch and merged into `main` with `--no-ff`:
`feature/app-skeleton` → `feature/core-settings` → `feature/device-selection` →
`feature/camera-bubble` → `feature/screen-recording` → `docs/readme`.
