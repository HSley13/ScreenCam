cmake_minimum_required(VERSION 3.24)

project(ScreenCam
    VERSION 0.2.0
    DESCRIPTION "Cross-platform screen recorder with a draggable camera bubble"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Lets clangd (nvim) understand the project: it reads build/compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(SCREENCAM_BUILD_APP "Build the Qt application (needs Qt 6.8 or newer)" ON)
option(SCREENCAM_BUILD_TESTS "Build the unit tests" ON)

if(MSVC)
    set(SCREENCAM_WARNINGS /W4 /permissive-)
else()
    set(SCREENCAM_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Portable core: plain C++20, no Qt -> geometry, sizes, pixel conversion.
# ---------------------------------------------------------------------------
add_library(screencam_core STATIC
    src/core/AudioLevel.cpp
    src/core/BubbleGeometry.cpp
    src/core/CameraLayout.cpp
    src/core/EncoderPlan.cpp
    src/core/Nv12.cpp
    src/core/Resolution.cpp
    src/core/TimeFormat.cpp
)
target_include_directories(screencam_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(screencam_core PRIVATE ${SCREENCAM_WARNINGS})

if(SCREENCAM_BUILD_TESTS)
    enable_testing()
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE screencam_core)
    add_test(NAME core_tests COMMAND core_tests)
endif()

# ---------------------------------------------------------------------------
# Qt application: C++ backend (capture, compositing, encoding) + QML front end.
# ---------------------------------------------------------------------------
if(SCREENCAM_BUILD_APP)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Multimedia)
    qt_standard_project_setup(REQUIRES 6.8)

    # Media library: compositing + encoding, Qt-only (no QML) so the tests can link it.
    add_library(screencam_media STATIC
        src/media/BubbleStyleSnapshot.cpp src/media/BubbleStyleSnapshot.hpp
        src/media/FrameCompositor.cpp src/media/FrameCompositor.hpp
        src/media/RecordingPipeline.cpp src/media/RecordingPipeline.hpp
    )
    target_include_directories(screencam_media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(screencam_media PUBLIC screencam_core Qt6::Gui Qt6::Multimedia)
    target_compile_options(screencam_media PRIVATE ${SCREENCAM_WARNINGS})

    if(SCREENCAM_BUILD_TESTS)
        find_package(Qt6 REQUIRED COMPONENTS Test)
        # Media tests run without a display (offscreen platform) so they also work in CI.
        function(screencam_add_qt_test name)
            qt_add_executable(${name} tests/${name}.cpp)
            target_link_libraries(${name} PRIVATE screencam_media Qt6::Test)
            add_test(NAME ${name} COMMAND ${name})
            set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        endfunction()
        screencam_add_qt_test(tst_compositor)
        screencam_add_qt_test(tst_pipeline)
    endif()

    set(SCREENCAM_APP_SOURCES
        src/app/AudioLevelMeter.cpp src/app/AudioLevelMeter.hpp
        src/app/BubbleStyle.cpp src/app/BubbleStyle.hpp
        src/app/CaptureSources.cpp src/app/CaptureSources.hpp
        src/app/DeviceCatalog.cpp src/app/DeviceCatalog.hpp
        src/app/RecordingSettings.cpp src/app/RecordingSettings.hpp
    )
    set(SCREENCAM_QML_FILES
        qml/BubbleSection.qml
        qml/CameraBubble.qml
        qml/Main.qml
        qml/PreviewStage.qml
        qml/SettingsPanel.qml
        qml/Theme.qml
        qml/components/Field.qml
        qml/components/LevelMeter.qml
        qml/components/SegmentedControl.qml
        qml/components/ShapePicker.qml
        qml/components/SidebarSection.qml
        qml/components/StyledButton.qml
        qml/components/StyledComboBox.qml
        qml/components/StyledSlider.qml
        qml/components/StyledSwitch.qml
    )
    set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

    qt_add_executable(ScreenCam WIN32 MACOSX_BUNDLE src/app/main.cpp)
    qt_add_qml_module(ScreenCam
        URI ScreenCam
        VERSION 1.0
        # Keep the module's generated files out of the way of the "ScreenCam" executable,
        # which would otherwise collide with a "ScreenCam/" directory in the build tree.
        OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/ScreenCam
        # C++ types use QtQuick value types (color, size, rect): tell the tooling about it.
        DEPENDENCIES QtQuick
        QML_FILES ${SCREENCAM_QML_FILES}
        SOURCES ${SCREENCAM_APP_SOURCES}
    )
    # src/app too: the generated QML type registrations include the headers by file name.
    target_include_directories(ScreenCam PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/app)
    target_compile_definitions(ScreenCam PRIVATE SCREENCAM_VERSION="${PROJECT_VERSION}")
    target_link_libraries(ScreenCam PRIVATE
        screencam_media
        Qt6::Widgets
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Multimedia)

    set_target_properties(ScreenCam PROPERTIES
        # macOS: camera/microphone usage strings live in this Info.plist template.
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.hsley13.screencam"
        MACOSX_BUNDLE_BUNDLE_NAME "ScreenCam"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
endif()
