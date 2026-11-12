cmake_minimum_required(VERSION 3.24)

# Must be set before project() so CMake passes -mmacosx-version-min to every compiler.
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")

project(ScreenCam
    VERSION 0.1.0
    DESCRIPTION "Screen recorder with a draggable camera bubble"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Lets clangd (nvim) understand the project: it reads build/compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Portable core: plain C++20, no Apple frameworks -> builds and tests anywhere
# ---------------------------------------------------------------------------
add_library(screencam_core STATIC
    src/core/Resolution.cpp
    src/core/CameraLayout.cpp
    src/core/EncoderPlan.cpp
    src/core/TimeFormat.cpp
)
target_include_directories(screencam_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(screencam_core PRIVATE -Wall -Wextra -Wpedantic)

option(SCREENCAM_BUILD_TESTS "Build the core unit tests" ON)
if(SCREENCAM_BUILD_TESTS)
    enable_testing()
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE screencam_core)
    add_test(NAME core_tests COMMAND core_tests)
endif()

# ---------------------------------------------------------------------------
# macOS application bundle (Objective-C++ glue around AppKit / AVFoundation)
# ---------------------------------------------------------------------------
if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

    set(SCREENCAM_APP_SOURCES
        src/app/main.mm
        src/app/AppDelegate.mm
        src/platform/CameraCapture.mm
        src/platform/ClockSync.mm
        src/platform/DeviceCatalog.mm
        src/platform/MicCapture.mm
        src/platform/ScreenCapture.mm
        src/recording/MovieWriter.mm
        src/recording/RecordingSession.mm
        src/ui/CameraBubbleWindow.mm
        src/ui/ControlPanelController.mm
    )

    add_executable(ScreenCam MACOSX_BUNDLE ${SCREENCAM_APP_SOURCES})
    target_include_directories(ScreenCam PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(ScreenCam PRIVATE
        $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>
        -Wall -Wextra -Wno-unused-parameter)
    target_link_libraries(ScreenCam PRIVATE
        screencam_core
        "-framework AppKit"
        "-framework AVFoundation"
        "-framework CoreGraphics"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework QuartzCore"
        "-framework ScreenCaptureKit")

    set_target_properties(ScreenCam PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.hsley13.screencam"
        MACOSX_BUNDLE_BUNDLE_NAME "ScreenCam"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

    # macOS only shows the camera / microphone / screen-recording prompts for a
    # *signed* bundle whose Info.plist is sealed into the signature. An ad-hoc
    # signature ("-") is enough for local development.
    add_custom_command(TARGET ScreenCam POST_BUILD
        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:ScreenCam>"
        COMMENT "Ad-hoc signing ScreenCam.app"
        VERBATIM)
else()
    message(STATUS "Not building on macOS: the ScreenCam app target is skipped.")
endif()
