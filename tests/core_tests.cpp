// Dependency-free unit tests for the portable core. Run with `ctest --test-dir build`.
#include <iostream>
#include <string>

#include "core/CameraLayout.hpp"
#include "core/EncoderPlan.hpp"
#include "core/Resolution.hpp"
#include "core/TimeFormat.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << "FAILED (line " << line << "): " << expression << '\n';
        g_failures++;
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

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

}  // namespace

int main() {
    testMakeEven();
    testCaptureSize();
    testCameraRecordingSize();
    testEncoderPlan();
    testFormatElapsed();

    if (g_failures == 0) {
        std::cout << "All core tests passed\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
}
