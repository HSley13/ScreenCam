// Dependency-free unit tests for the portable core. Run with `ctest --test-dir build`.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "core/AudioLevel.hpp"
#include "core/BubbleGeometry.hpp"
#include "core/CameraLayout.hpp"
#include "core/Nv12.hpp"
#include "core/EncoderPlan.hpp"
#include "core/Resolution.hpp"
#include "core/TimeFormat.hpp"
#include "core/Pointer.hpp"
#include "core/Timeline.hpp"
#include "core/WindowFilter.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << "FAILED (line " << line << "): " << expression << '\n';
        g_failures++;
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

bool near(double a, double b, double tolerance = 1e-6) {
    return std::abs(a - b) <= tolerance;
}

using namespace screencam::core;

void testMakeEven() {
    CHECK(makeEven(1080) == 1080);
    CHECK(makeEven(1081) == 1080);
    CHECK(makeEven(1) == 2);
}

void testCaptureSize() {
    const PixelSize macbook14{3024, 1964};
    CHECK((computeCaptureSize(macbook14, ResolutionPreset::Native) == PixelSize{3024, 1964}));
    // 3024 * 720 / 1964 = 1108.6 -> 1109 -> even 1108
    CHECK((computeCaptureSize(macbook14, ResolutionPreset::HD720) == PixelSize{1108, 720}));

    const PixelSize qhd{2560, 1440};
    CHECK((computeCaptureSize(qhd, ResolutionPreset::FullHD1080) == PixelSize{1920, 1080}));
    CHECK((computeCaptureSize(qhd, ResolutionPreset::QHD1440) == PixelSize{2560, 1440}));

    // Never upscale: 4K requested on a 1080p display stays 1080p.
    const PixelSize fullHd{1920, 1080};
    CHECK((computeCaptureSize(fullHd, ResolutionPreset::UHD2160) == PixelSize{1920, 1080}));

    CHECK((computeCaptureSize(PixelSize{0, 0}, ResolutionPreset::Native) == PixelSize{0, 0}));
}

void testCameraRecordingSize() {
    const PixelSize webcam{1920, 1080};
    CHECK((cameraRecordingSize(webcam, CameraOrientation::Landscape) == PixelSize{1920, 1080}));
    // 1080 * 9 / 16 = 607.5 -> 608
    CHECK((cameraRecordingSize(webcam, CameraOrientation::Portrait) == PixelSize{608, 1080}));

    const PixelSize fourByThree{1280, 960};
    CHECK((cameraRecordingSize(fourByThree, CameraOrientation::Landscape) == PixelSize{1280, 720}));
    CHECK((cameraRecordingSize(fourByThree, CameraOrientation::Portrait) == PixelSize{540, 960}));

    // A portrait camera (iPhone Continuity Camera held upright) asked for landscape.
    const PixelSize upright{1080, 1920};
    CHECK((cameraRecordingSize(upright, CameraOrientation::Landscape) == PixelSize{1080, 608}));

    CHECK((cameraRecordingSize(PixelSize{}, CameraOrientation::Portrait) == PixelSize{0, 0}));
}

void testEncoderPlan() {
    CHECK(!requiresHevc(PixelSize{3840, 2160}));
    CHECK(!requiresHevc(PixelSize{2160, 3840}));
    CHECK(requiresHevc(PixelSize{5120, 2880}));

    const EncoderPlan hd = planEncoder(PixelSize{1920, 1080}, 30);
    CHECK(hd.codec == VideoCodec::H264);
    CHECK(hd.averageBitRate == 6'220'800);  // 1920*1080*30*0.1
    CHECK(hd.maxKeyFrameInterval == 60);

    const EncoderPlan tiny = planEncoder(PixelSize{640, 360}, 30);
    CHECK(tiny.averageBitRate == 2'000'000);  // clamped up to the minimum

    const EncoderPlan fiveK = planEncoder(PixelSize{5120, 2880}, 30);
    CHECK(fiveK.codec == VideoCodec::HEVC);
    CHECK(fiveK.averageBitRate == 30'965'760);  // 5120*2880*30*0.07

    CHECK(planEncoder(PixelSize{1920, 1080}, 0).framesPerSecond == 30);
}

void testFormatElapsed() {
    CHECK(formatElapsed(0) == "00:00");
    CHECK(formatElapsed(42.9) == "00:42");
    CHECK(formatElapsed(723) == "12:03");
    CHECK(formatElapsed(3723) == "1:02:03");
    CHECK(formatElapsed(-5) == "00:00");
}


void testBubbleSize() {
    const SizeD canvas{1920, 1080};  // short side 1080
    const SizeD circle = bubbleSize(canvas, BubbleShape::Circle, 0.25);
    CHECK(near(circle.width, 270) && near(circle.height, 270));

    // Rectangles: long side = 1.25 * 270 = 337.5
    const SizeD portrait = bubbleSize(canvas, BubbleShape::Portrait, 0.25);
    CHECK(near(portrait.height, 337.5) && near(portrait.width, 337.5 * 9.0 / 16.0));
    const SizeD landscape = bubbleSize(canvas, BubbleShape::Landscape, 0.25);
    CHECK(near(landscape.width, 337.5) && near(landscape.height, 337.5 * 9.0 / 16.0));

    // Size fraction is clamped to [0.08, 0.6].
    CHECK(near(bubbleSize(canvas, BubbleShape::Circle, 5.0).width, 1080 * 0.6));
    CHECK(near(bubbleSize(canvas, BubbleShape::Circle, 0.0).width, 1080 * 0.08));
}

void testCornerRadius() {
    const SizeD square{200, 200};
    CHECK(near(bubbleCornerRadius(BubbleShape::Circle, square, 0.0), 100));  // circle ignores roundness
    CHECK(near(bubbleCornerRadius(BubbleShape::Squircle, square, 0.5), 50));
    CHECK(near(bubbleCornerRadius(BubbleShape::Portrait, SizeD{90, 160}, 1.0), 45));  // half the short side
    CHECK(near(bubbleCornerRadius(BubbleShape::Squircle, square, 3.0), 100));  // roundness clamped
}

void testBubbleRectClamping() {
    const SizeD canvas{1000, 1000};  // margin = 30
    // Centred bubble stays centred.
    const RectD centred = bubbleRect(canvas, BubbleShape::Circle, 0.2, PointD{0.5, 0.5});
    CHECK(near(centred.x, 400) && near(centred.y, 400) && near(centred.width, 200));

    // Dragged past the bottom-right corner: pushed back inside, 30 px from both edges.
    const RectD corner = bubbleRect(canvas, BubbleShape::Circle, 0.2, PointD{1.5, 2.0});
    CHECK(near(corner.x + corner.width, 970) && near(corner.y + corner.height, 970));

    // Dragged past the top-left corner.
    const RectD topLeft = bubbleRect(canvas, BubbleShape::Circle, 0.2, PointD{-1.0, -1.0});
    CHECK(near(topLeft.x, 30) && near(topLeft.y, 30));
}

void testSnapping() {
    const SizeD canvas{1000, 1000};
    const SizeD bubble{200, 200};
    // Bottom-left anchor centre = (30 + 100) / 1000 = 0.13; (1000 - 130) / 1000 = 0.87
    const PointD nearCorner = snapNormalizedCenter(canvas, bubble, PointD{0.16, 0.84}, 0.05);
    CHECK(near(nearCorner.x, 0.13) && near(nearCorner.y, 0.87));

    // Close to the middle on x only: snaps x, leaves y alone.
    const PointD nearMiddle = snapNormalizedCenter(canvas, bubble, PointD{0.52, 0.35}, 0.05);
    CHECK(near(nearMiddle.x, 0.5) && near(nearMiddle.y, 0.35));

    // Far from every anchor: unchanged.
    const PointD free = snapNormalizedCenter(canvas, bubble, PointD{0.3, 0.7}, 0.05);
    CHECK(near(free.x, 0.3) && near(free.y, 0.7));
}

void testAspectFillCrop() {
    // 16:9 camera into a circle (1:1): keep full height, cut the sides.
    const RectD square = aspectFillCrop(SizeD{1920, 1080}, 1.0);
    CHECK(near(square.x, 420) && near(square.y, 0) && near(square.width, 1080) && near(square.height, 1080));

    // 16:9 camera into a portrait bubble (9:16): 1080 * 9/16 = 607.5 wide.
    const RectD portrait = aspectFillCrop(SizeD{1920, 1080}, 9.0 / 16.0);
    CHECK(near(portrait.width, 607.5) && near(portrait.x, (1920 - 607.5) / 2));

    // Portrait source into a landscape bubble: keep full width, cut top and bottom.
    const RectD landscape = aspectFillCrop(SizeD{1080, 1920}, 16.0 / 9.0);
    CHECK(near(landscape.width, 1080) && near(landscape.height, 607.5));
}

void testAspectFitRect() {
    // Same aspect: fills the target exactly.
    const RectD same = aspectFitRect(SizeD{1920, 1080}, SizeD{1280, 720});
    CHECK(near(same.x, 0) && near(same.y, 0) && near(same.width, 1280) && near(same.height, 720));
    // A square window in a 16:9 video: pillarboxed in the middle.
    const RectD square = aspectFitRect(SizeD{800, 800}, SizeD{1600, 900});
    CHECK(near(square.width, 900) && near(square.height, 900) && near(square.x, 350) && near(square.y, 0));
    // A wide window in a 16:9 video: letterboxed top and bottom.
    const RectD wide = aspectFitRect(SizeD{2000, 500}, SizeD{1600, 900});
    CHECK(near(wide.width, 1600) && near(wide.height, 400) && near(wide.y, 250));
}

void testYuvColours() {
    const YuvPixel black = rgbToYuv709(0, 0, 0);
    CHECK(black.y == 16 && black.u == 128 && black.v == 128);
    const YuvPixel white = rgbToYuv709(255, 255, 255);
    CHECK(white.y == 235 && white.u == 128 && white.v == 128);
    const YuvPixel red = rgbToYuv709(255, 0, 0);  // reference BT.709: Y 63, U 102, V 240
    CHECK(red.y == 63 && red.u == 102 && red.v == 240);
}

void testNv12Conversion() {
    // 4x2 image: left half pure red, right half pure blue (0xAARRGGBB words).
    const int width = 4;
    const int height = 2;
    std::vector<std::uint32_t> pixels{0xFFFF0000, 0xFFFF0000, 0xFF0000FF, 0xFF0000FF,
                                      0xFFFF0000, 0xFFFF0000, 0xFF0000FF, 0xFF0000FF};
    std::vector<std::uint8_t> yPlane(width * height);
    std::vector<std::uint8_t> uvPlane(width * height / 2);
    convertXrgbToNv12(reinterpret_cast<const std::uint8_t *>(pixels.data()), width * 4, width, height, yPlane.data(),
                      width, uvPlane.data(), width);

    const YuvPixel red = rgbToYuv709(255, 0, 0);
    const YuvPixel blue = rgbToYuv709(0, 0, 255);
    CHECK(yPlane[0] == red.y && yPlane[3] == blue.y && yPlane[4] == red.y && yPlane[7] == blue.y);
    // One UV pair per 2x2 block: block 0 is all red, block 1 all blue.
    CHECK(uvPlane[0] == red.u && uvPlane[1] == red.v);
    CHECK(uvPlane[2] == blue.u && uvPlane[3] == blue.v);
}

void testAudioLevel() {
    CHECK(near(peakToMeterLevel(0.0), 0.0));
    CHECK(near(peakToMeterLevel(1.0), 1.0));
    CHECK(near(peakToMeterLevel(0.1), 2.0 / 3.0));  // -20 dBFS
    CHECK(near(peakToMeterLevel(0.0001), 0.0));     // -80 dBFS is below the floor
    CHECK(near(smoothMeterLevel(0.8, 0.2, 0.25), 0.6));  // decays from 0.8
    CHECK(near(smoothMeterLevel(0.3, 0.9, 0.25), 0.9));  // jumps up immediately
}

void testTimeline() {
    const std::vector<std::int64_t> starts{0, 5000, 12000};
    CHECK(segmentAt(starts, 0) == 0);
    CHECK(segmentAt(starts, 4999) == 0);
    CHECK(segmentAt(starts, 5000) == 1);   // a segment owns its start time
    CHECK(segmentAt(starts, 11999) == 1);
    CHECK(segmentAt(starts, 60000) == 2);  // after the last cut: last segment
    CHECK(segmentAt(starts, -10) == 0);
    CHECK(segmentAt({}, 100) == -1);

    // 30 s video, segments must stay >= 500 ms.
    CHECK(canSplitAt(starts, 8000, 30000, 500));
    CHECK(!canSplitAt(starts, 5200, 30000, 500));   // too close to the cut at 5000
    CHECK(!canSplitAt(starts, 200, 30000, 500));    // too close to the start
    CHECK(!canSplitAt(starts, 29800, 30000, 500));  // too close to the end

    CHECK(insertionIndex(starts, 8000) == 2);
    CHECK(insertionIndex(starts, 20000) == 3);
}

void testWindowFilter() {
    WindowTraits safari;  // a normal app window
    safari.width = 1200;
    safari.height = 800;
    CHECK(isUserAppWindow(safari));

    WindowTraits menuBar = safari;  // menu bar / status items / Dock live above level 0
    menuBar.layer = 24;
    CHECK(!isUserAppWindow(menuBar));

    WindowTraits clock = safari;  // a status item: tiny
    clock.width = 60;
    clock.height = 24;
    CHECK(!isUserAppWindow(clock));

    WindowTraits cpuMeterPopover = safari;  // menu-bar utility (no Dock icon), even if big
    cpuMeterPopover.regularApp = false;
    CHECK(!isUserAppWindow(cpuMeterPopover));

    WindowTraits palette = safari;
    palette.toolWindow = true;
    CHECK(!isUserAppWindow(palette));

    WindowTraits otherDesktop = safari;
    otherDesktop.cloaked = true;
    CHECK(!isUserAppWindow(otherDesktop));

    WindowTraits invisible = safari;
    invisible.alpha = 0.0;
    CHECK(!isUserAppWindow(invisible));

    WindowTraits frameless = safari;  // no minimize/maximize: splash screens, overlays
    frameless.hasWindowControls = false;
    CHECK(!isUserAppWindow(frameless));

    WindowTraits own = safari;
    own.ownedBySelf = true;
    CHECK(!isUserAppWindow(own));

    CHECK(windowLabel("Safari", "GitHub") == "Safari \u2014 GitHub");
    CHECK(windowLabel("Finder", "") == "Finder");
    CHECK(windowLabel("Notes", "Notes") == "Notes");
    CHECK(windowLabel("", "Untitled") == "Untitled");
}

void testPointerPlacement() {
    // A 1512 x 982 screen whose top-left is at (100, 50) on the desktop.
    const RectD screen{100.0, 50.0, 1512.0, 982.0};
    const PointerPlacement middle = placePointer(PointD{100.0 + 756.0, 50.0 + 491.0}, screen);
    CHECK(middle.visible);
    CHECK(std::abs(middle.tip.x - 0.5) < 1e-9);
    CHECK(std::abs(middle.tip.y - 0.5) < 1e-9);
    CHECK(std::abs(middle.height - kPointerHeightPoints / 982.0) < 1e-12);

    CHECK(placePointer(PointD{100.0, 50.0}, screen).visible);           // top-left corner
    CHECK(!placePointer(PointD{99.0, 60.0}, screen).visible);           // just left of it
    CHECK(!placePointer(PointD{1612.0, 60.0}, screen).visible);         // right edge: next monitor
    CHECK(!placePointer(PointD{500.0, 500.0}, RectD{}).visible);        // no target
}

}  // namespace

int main() {
    testMakeEven();
    testCaptureSize();
    testCameraRecordingSize();
    testEncoderPlan();
    testFormatElapsed();
    testBubbleSize();
    testCornerRadius();
    testBubbleRectClamping();
    testSnapping();
    testAspectFillCrop();
    testAspectFitRect();
    testYuvColours();
    testNv12Conversion();
    testAudioLevel();
    testTimeline();
    testWindowFilter();
    testPointerPlacement();

    if (g_failures == 0) {
        std::cout << "All core tests passed\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
}
