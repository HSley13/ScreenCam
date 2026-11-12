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
    src/core/Timeline.cpp
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
    endif()

    set(SCREENCAM_APP_SOURCES
        src/app/AudioLevelMeter.cpp src/app/AudioLevelMeter.hpp
        src/app/BubbleStyle.cpp src/app/BubbleStyle.hpp
        src/app/CaptureSources.cpp src/app/CaptureSources.hpp
        src/app/DeviceCatalog.cpp src/app/DeviceCatalog.hpp
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

    if(APPLE)
        enable_language(OBJCXX)  # WindowTools_mac.mm talks to NSWindow
        list(APPEND SCREENCAM_PLATFORM_FILES src/app/WindowTools_mac.mm)
    endif()

    qt_add_executable(ScreenCam WIN32 MACOSX_BUNDLE src/app/main.cpp ${SCREENCAM_PLATFORM_FILES})
    if(APPLE)
        target_link_libraries(ScreenCam PRIVATE "-framework AppKit")
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
        set_tests_properties(tst_project_editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

        # Editor smoke test: record a tiny synthetic project first (fixture), then open it.
        qt_add_executable(make_sample_project tests/make_sample_project.cpp)
        target_link_libraries(make_sample_project PRIVATE screencam_media)
        set(SCREENCAM_SAMPLE_PROJECT ${CMAKE_CURRENT_BINARY_DIR}/test-data/Sample.screencam)
        add_test(NAME make_sample_project COMMAND make_sample_project ${SCREENCAM_SAMPLE_PROJECT} 3)
        add_test(NAME ui_smoke_editor COMMAND ScreenCam --self-test --open ${SCREENCAM_SAMPLE_PROJECT})
        set_tests_properties(make_sample_project PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen" FIXTURES_SETUP sample_project)
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
