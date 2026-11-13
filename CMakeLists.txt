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
option(SCREENCAM_ENABLE_EXPORT "Video export from the editor (needs the FFmpeg development files)" ON)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Sources are UTF-8 (labels like "Safari — GitHub", "▸"); MSVC would otherwise read them in
# the system's code page and garble those strings.
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

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
    src/core/Timeline.cpp
    src/core/WindowFilter.cpp
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

    # Screen capture (QScreenCapture) and recording composited frames (QVideoFrameInput) only
    # exist in Qt Multimedia's FFmpeg backend. Qt builds without it (Homebrew's, or Qt compiled
    # from source without -ffmpeg-dir) build fine but show an empty screen preview at runtime.
    if(NOT TARGET Qt6::QFFmpegMediaPlugin)
        message(WARNING
            "The Qt found at ${Qt6_DIR} has no FFmpeg multimedia backend (Qt6::QFFmpegMediaPlugin).\n"
            "ScreenCam will build, but it cannot capture the screen or record.\n"
            "Fix: use Qt from the Qt Online Installer, or rebuild Qt with '-ffmpeg-dir <ffmpeg-prefix>'.\n"
            "See README.md > Troubleshooting.")
    endif()

    # Media library: compositing + encoding, Qt-only (no QML) so the tests can link it.
    add_library(screencam_media STATIC
        src/media/BubbleStyleSnapshot.cpp src/media/BubbleStyleSnapshot.hpp
        src/media/FrameCompositor.cpp src/media/FrameCompositor.hpp
        src/media/MediaBackendCheck.cpp src/media/MediaBackendCheck.hpp
        src/media/Project.cpp src/media/Project.hpp
        src/media/RecordingPipeline.cpp src/media/RecordingPipeline.hpp
    )
    target_include_directories(screencam_media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(screencam_media PUBLIC screencam_core Qt6::Gui Qt6::Multimedia)
    target_compile_options(screencam_media PRIVATE ${SCREENCAM_WARNINGS})

    # Export library: turns an edited project into a finished MP4 by talking to FFmpeg's
    # libraries directly (decode -> composite -> encode, faster than real time). Optional:
    # without the FFmpeg headers the app still records and edits, it just can't export.
    set(SCREENCAM_HAS_EXPORT OFF)
    if(SCREENCAM_ENABLE_EXPORT)
        find_package(FFmpeg 59)  # libavcodec 59 = FFmpeg 5.0
    endif()
    if(FFmpeg_FOUND)
        set(SCREENCAM_HAS_EXPORT ON)
        add_library(screencam_export STATIC
            src/export/FFmpegUtil.hpp
            src/export/ProjectExporter.cpp src/export/ProjectExporter.hpp
            src/export/VideoFileReader.cpp src/export/VideoFileReader.hpp
        )
        target_link_libraries(screencam_export PUBLIC
            screencam_media FFmpeg::avformat FFmpeg::avcodec FFmpeg::swscale FFmpeg::avutil)
        target_compile_options(screencam_export PRIVATE ${SCREENCAM_WARNINGS})
        # Whoever links the exporter (the app, tests) gets the define that turns export on.
        target_compile_definitions(screencam_export PUBLIC SCREENCAM_HAS_EXPORT)
        message(STATUS "Video export: ON (FFmpeg ${FFmpeg_VERSION} in ${FFmpeg_INCLUDE_DIR})")
    elseif(SCREENCAM_ENABLE_EXPORT)
        message(STATUS "Video export: OFF - FFmpeg development files not found. "
                       "Pass -DFFMPEG_DIR=<prefix> (see README > Export) to enable it.")
    else()
        message(STATUS "Video export: OFF (SCREENCAM_ENABLE_EXPORT=OFF)")
    endif()

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
        screencam_add_qt_test(tst_media_backend)
        screencam_add_qt_test(tst_project)

        # A tiny real project (synthetic screen + camera) that the editor and export tests open.
        qt_add_executable(make_sample_project tests/make_sample_project.cpp)
        target_link_libraries(make_sample_project PRIVATE screencam_media)
        set(SCREENCAM_SAMPLE_PROJECT ${CMAKE_CURRENT_BINARY_DIR}/test-data/Sample.screencam)
        add_test(NAME make_sample_project COMMAND make_sample_project ${SCREENCAM_SAMPLE_PROJECT} 3)
        set_tests_properties(make_sample_project PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen" FIXTURES_SETUP sample_project)

        if(SCREENCAM_HAS_EXPORT)
            qt_add_executable(tst_export tests/tst_export.cpp)
            target_link_libraries(tst_export PRIVATE screencam_export Qt6::Test)
            add_test(NAME tst_export COMMAND tst_export)
            set_tests_properties(tst_export PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SCREENCAM_SAMPLE_PROJECT=${SCREENCAM_SAMPLE_PROJECT}"
                FIXTURES_REQUIRED sample_project)
        endif()
    endif()

    set(SCREENCAM_APP_SOURCES
        src/app/AppWindows.hpp
        src/app/AudioLevelMeter.cpp src/app/AudioLevelMeter.hpp
        src/app/BubbleStyle.cpp src/app/BubbleStyle.hpp
        src/app/CaptureSources.cpp src/app/CaptureSources.hpp
        src/app/DeviceCatalog.cpp src/app/DeviceCatalog.hpp
        src/app/ExcludingScreenCapture.hpp
        src/app/ProjectEditor.cpp src/app/ProjectEditor.hpp
        src/app/RecordingController.cpp src/app/RecordingController.hpp
        src/app/RecordingSettings.cpp src/app/RecordingSettings.hpp
        src/app/WindowTools.cpp src/app/WindowTools.hpp
    )
    set(SCREENCAM_QML_FILES
        qml/BubbleSection.qml
        qml/BubbleView.qml
        qml/CameraBubble.qml
        qml/ControlBar.qml
        qml/CountdownOverlay.qml
        qml/EditorInspector.qml
        qml/EditorPage.qml
        qml/EditorTimeline.qml
        qml/ErrorBanner.qml
        qml/FloatingBubble.qml
        qml/Main.qml
        qml/MiniControlBar.qml
        qml/RecordingControls.qml
        qml/RecordingOutline.qml
        qml/PreviewStage.qml
        qml/RegionSelector.qml
        qml/SettingsPanel.qml
        qml/Theme.qml
        qml/components/Field.qml
        qml/components/LevelMeter.qml
        qml/components/RecordButton.qml
        qml/components/SegmentedControl.qml
        qml/components/ShapePicker.qml
        qml/components/SidebarSection.qml
        qml/components/StyledButton.qml
        qml/components/StyledComboBox.qml
        qml/components/StyledSlider.qml
        qml/components/StyledSwitch.qml
    )
    set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

    # Platform icons: .icns inside the macOS bundle, .rc/.ico for the Windows .exe.
    set(SCREENCAM_PLATFORM_FILES)
    if(APPLE)
        set(SCREENCAM_PLATFORM_FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/ScreenCam.icns)
        set_source_files_properties(${SCREENCAM_PLATFORM_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    elseif(WIN32)
        set(SCREENCAM_PLATFORM_FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/ScreenCam.rc)
    endif()

    # Native window code: hiding our own windows from capture (WindowTools) and listing
    # the real app windows for "Record ▸ Window" (AppWindows).
    set(SCREENCAM_PLATFORM_LIBS)
    if(APPLE)
        enable_language(OBJCXX)  # these .mm files talk to AppKit (NSWindow, NSRunningApplication)
        list(APPEND SCREENCAM_PLATFORM_FILES
            src/app/WindowTools_mac.mm src/app/AppWindows_mac.mm src/app/ExcludingScreenCapture_mac.mm)
        # ScreenCaptureKit (macOS 12.3+) is weak-linked: the app still starts on 12.0-12.2 and
        # falls back to Qt's screen capture there.
        list(APPEND SCREENCAM_PLATFORM_LIBS "-framework AppKit" "-framework CoreGraphics" "-framework CoreMedia"
                                            "-framework CoreVideo" "-weak_framework ScreenCaptureKit")
        # Automatic reference counting for the ScreenCaptureKit code (streams, outputs, blocks).
        set_source_files_properties(src/app/ExcludingScreenCapture_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    elseif(WIN32)
        list(APPEND SCREENCAM_PLATFORM_FILES src/app/AppWindows_win.cpp src/app/ExcludingScreenCapture_other.cpp)
        list(APPEND SCREENCAM_PLATFORM_LIBS dwmapi version)  # cloaked windows; app names
    else()
        find_package(X11)
        if(X11_FOUND)
            list(APPEND SCREENCAM_PLATFORM_FILES src/app/AppWindows_x11.cpp)
            list(APPEND SCREENCAM_PLATFORM_LIBS X11::X11)
        else()
            list(APPEND SCREENCAM_PLATFORM_FILES src/app/AppWindows_other.cpp)
        endif()
        list(APPEND SCREENCAM_PLATFORM_FILES src/app/ExcludingScreenCapture_other.cpp)
    endif()

    qt_add_executable(ScreenCam WIN32 MACOSX_BUNDLE src/app/main.cpp ${SCREENCAM_PLATFORM_FILES})
    target_link_libraries(ScreenCam PRIVATE ${SCREENCAM_PLATFORM_LIBS})

    # The native id inside a QCapturableWindow (to match it with the OS's window list) is
    # only reachable through Qt Multimedia's private header. Qt 6.9+ ships private targets
    # as a separate package; in 6.8 Qt6::MultimediaPrivate comes with Multimedia.
    # (We use one small, stable struct from it — unchanged from 6.8 to 6.12 — and fall back to
    # Qt's unfiltered list without it, hence the warning is switched off.)
    set(QT_NO_PRIVATE_MODULE_WARNING ON)
    find_package(Qt6 QUIET OPTIONAL_COMPONENTS MultimediaPrivate)
    if(TARGET Qt6::MultimediaPrivate)
        target_link_libraries(ScreenCam PRIVATE Qt6::MultimediaPrivate)
        target_compile_definitions(ScreenCam PRIVATE SCREENCAM_HAS_WINDOW_IDS)
    else()
        message(WARNING "Qt Multimedia's private headers weren't found: the window picker will "
                        "list every window Qt reports (menu bar items included).")
    endif()
    qt_add_qml_module(ScreenCam
        URI ScreenCam
        VERSION 1.0
        # Keep the module's generated files out of the way of the "ScreenCam" executable,
        # which would otherwise collide with a "ScreenCam/" directory in the build tree.
        OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/ScreenCam
        # C++ types use QtQuick value types (color, size, rect): tell the tooling about it.
        DEPENDENCIES QtQuick
        QML_FILES ${SCREENCAM_QML_FILES}
        RESOURCES
            qml/icons/tray-paused.png
            qml/icons/tray-recording.png
        SOURCES ${SCREENCAM_APP_SOURCES}
    )
    # src/app too: the generated QML type registrations include the headers by file name.
    target_include_directories(ScreenCam PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/app)
    target_compile_definitions(ScreenCam PRIVATE SCREENCAM_VERSION="${PROJECT_VERSION}")
    if(SCREENCAM_HAS_EXPORT)
        target_link_libraries(ScreenCam PRIVATE screencam_export)
    endif()
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
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "ScreenCam.icns")

    # macOS remembers Camera / Microphone / Screen Recording permission per *code signature*.
    # The linker's automatic ad-hoc signature is a hash of the binary, so every rebuild looks
    # like a new app and the permission you granted no longer applies. Sign every build with one
    # fixed identity instead (scripts/create-macos-signing-identity.sh creates
    # "ScreenCam Development" once); ad hoc ("-") is only the fallback.
    if(APPLE)
        set(SCREENCAM_SIGNING_NAME "ScreenCam Development")
        execute_process(COMMAND security find-identity -v -p codesigning
            OUTPUT_VARIABLE _screencam_identities ERROR_QUIET)
        if(_screencam_identities MATCHES "\"${SCREENCAM_SIGNING_NAME}\"")
            set(_screencam_found_identity "${SCREENCAM_SIGNING_NAME}")
        else()
            set(_screencam_found_identity "-")
        endif()
        set(SCREENCAM_CODESIGN_IDENTITY "${_screencam_found_identity}" CACHE STRING
            "codesign identity for ScreenCam.app ('-' = ad hoc)")
        # Upgrade an earlier ad-hoc choice once the identity exists.
        if(SCREENCAM_CODESIGN_IDENTITY STREQUAL "-" AND NOT _screencam_found_identity STREQUAL "-")
            set(SCREENCAM_CODESIGN_IDENTITY "${_screencam_found_identity}" CACHE STRING
                "codesign identity for ScreenCam.app ('-' = ad hoc)" FORCE)
        endif()
        if(SCREENCAM_CODESIGN_IDENTITY STREQUAL "-")
            message(WARNING
                "ScreenCam is signed ad hoc, so macOS forgets its Screen Recording / Camera / Microphone "
                "permission after every rebuild. Run scripts/create-macos-signing-identity.sh once, then "
                "reconfigure.")
        else()
            message(STATUS "ScreenCam: signing with '${SCREENCAM_CODESIGN_IDENTITY}'")
        endif()

        add_custom_command(TARGET ScreenCam POST_BUILD
            COMMAND codesign --force --sign "${SCREENCAM_CODESIGN_IDENTITY}"
                    --identifier com.hsley13.screencam "$<TARGET_BUNDLE_DIR:ScreenCam>"
            COMMENT "Signing ScreenCam.app (${SCREENCAM_CODESIGN_IDENTITY})"
            VERBATIM)
    endif()

    if(SCREENCAM_BUILD_TESTS)
        # Loads the whole QML UI headless and fails on any QML warning.
        add_test(NAME ui_smoke COMMAND ScreenCam --self-test)
        set_tests_properties(ui_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

        # The editor's model, built from the app sources it needs (they aren't a library).
        qt_add_executable(tst_project_editor tests/tst_project_editor.cpp
            src/app/ProjectEditor.cpp src/app/BubbleStyle.cpp)
        target_include_directories(tst_project_editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(tst_project_editor PRIVATE screencam_media Qt6::Qml Qt6::Test)
        add_test(NAME tst_project_editor COMMAND tst_project_editor)
        set_tests_properties(tst_project_editor PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SCREENCAM_SAMPLE_PROJECT=${SCREENCAM_SAMPLE_PROJECT}")
        if(SCREENCAM_HAS_EXPORT)
            target_link_libraries(tst_project_editor PRIVATE screencam_export)
            set_tests_properties(tst_project_editor PROPERTIES FIXTURES_REQUIRED sample_project)
        endif()

        # Window picker on Linux/X11, against a fake desktop on a virtual X server.
        find_program(SCREENCAM_XVFB_RUN xvfb-run)
        if(X11_FOUND AND SCREENCAM_XVFB_RUN AND TARGET Qt6::MultimediaPrivate)
            qt_add_executable(tst_app_windows tests/tst_app_windows.cpp src/app/AppWindows_x11.cpp)
            target_include_directories(tst_app_windows PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
            target_link_libraries(tst_app_windows PRIVATE
                screencam_core Qt6::Multimedia Qt6::MultimediaPrivate Qt6::Test X11::X11)
            add_test(NAME tst_app_windows COMMAND ${SCREENCAM_XVFB_RUN} -a $<TARGET_FILE:tst_app_windows>)
            set_tests_properties(tst_app_windows PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=xcb")
        endif()

        # Editor smoke test: opens the sample project (see make_sample_project above).
        add_test(NAME ui_smoke_editor COMMAND ScreenCam --self-test --open ${SCREENCAM_SAMPLE_PROJECT})
        set_tests_properties(ui_smoke_editor PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen" FIXTURES_REQUIRED sample_project)
    endif()

    # Window/taskbar icon at runtime (all platforms).
    qt_add_resources(ScreenCam "screencam_icon"
        PREFIX "/screencam"
        BASE "packaging/icons"
        FILES packaging/icons/screencam.png)

    # -----------------------------------------------------------------------
    # Install + deploy:  cmake --install build --prefix <dir>
    # Produces a self-contained app: Qt libraries, plugins (incl. the FFmpeg
    # multimedia backend) and QML modules are copied next to the executable by
    # Qt's deploy tooling (macdeployqt / windeployqt / Linux deploy).
    # -----------------------------------------------------------------------
    include(GNUInstallDirs)
    install(TARGETS ScreenCam
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    qt_generate_deploy_qml_app_script(
        TARGET ScreenCam
        OUTPUT_SCRIPT screencam_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT ${screencam_deploy_script})

    if(APPLE)
        # Deployment rewrites the binaries, which breaks their signature: re-sign the whole
        # bundle (with the same identity as the build, so granted permissions carry over).
        install(CODE "execute_process(COMMAND codesign --force --deep --sign \"${SCREENCAM_CODESIGN_IDENTITY}\" --identifier com.hsley13.screencam \"\${CMAKE_INSTALL_PREFIX}/ScreenCam.app\")")
    elseif(UNIX)
        install(FILES packaging/linux/screencam.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
        install(FILES packaging/icons/screencam.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)
    endif()

    # Packages:  cmake --build build --target package
    #   macOS -> .dmg (drag to Applications), Windows -> .zip, Linux -> .tar.gz
    set(CPACK_PACKAGE_NAME "ScreenCam")
    set(CPACK_PACKAGE_VENDOR "hsley13")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGE_FILE_NAME "ScreenCam-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
    if(APPLE)
        set(CPACK_GENERATOR "DragNDrop")
    elseif(WIN32)
        set(CPACK_GENERATOR "ZIP")
    else()
        set(CPACK_GENERATOR "TGZ")
    endif()
    include(CPack)
endif()
