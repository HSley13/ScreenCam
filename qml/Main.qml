pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform

ApplicationWindow {
    id: window

    width: 1240
    height: 780
    minimumWidth: 980
    minimumHeight: 620
    visible: true
    title: qsTr("ScreenCam")
    color: Theme.window

    // Set from the command line (`ScreenCam --open My.screencam`), see main.cpp.
    property string initialProject: ""
    // The project open in the editor; empty = the recorder is showing.
    property string editorFolder: ""
    readonly property bool editing: editorFolder.length > 0

    // macOS asks for camera/microphone access the first time; elsewhere this is a no-op.
    Component.onCompleted: {
        Devices.requestPermissions();
        if (initialProject.length > 0) {
            openEditor(initialProject);
        }
    }

    function openEditor(folder: string) {
        window.editorFolder = "";  // re-opening the same project reloads it
        window.editorFolder = folder;
        window.bringToFront();
    }
    // Never record ScreenCam's own window (this also stops the preview from showing itself
    // in an endless loop). Re-applied whenever the window is shown again.
    onVisibilityChanged: (visibility) => {
        if (visibility !== Window.Hidden) {
            WindowTools.excludeFromCapture(window);
        }
    }

    // Run the live inputs only while the window can actually be seen.
    Binding {
        target: Capture
        property: "previewEnabled"
        value: window.visible && window.visibility !== Window.Minimized && !window.editing
    }

    // --- Recorder -----------------------------------------------------------------------------
    RowLayout {
        visible: !window.editing
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            ErrorBanner {
                Layout.fillWidth: true
            }

            PreviewStage {
                id: stage
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            ControlBar {
                Layout.fillWidth: true
                onProjectChosen: (folder) => window.openEditor(folder)
            }
        }

        SettingsPanel {
            Layout.preferredWidth: Theme.sidebarWidth
            Layout.fillHeight: true
            locked: Recorder.busy

            BubbleSection {}
        }
    }

    // --- Editor (created only while a project is open) ----------------------------------------
    Loader {
        anchors.fill: parent
        anchors.margins: 16
        active: window.editing
        sourceComponent: EditorPage {
            folder: window.editorFolder
            onCloseRequested: window.editorFolder = ""
        }
    }

    CountdownOverlay {
        anchors.fill: parent
    }

    // Recording controls on their own, when there's no camera bubble to carry them.
    MiniControlBar {
        bubbleShown: floatingBubble.visible
    }

    // The name of the screen, window or region being recorded, shown on it.
    TargetTag {}

    // Your camera, floating on the desktop (only in Screen + Camera mode), with the
    // recording controls attached while recording.
    FloatingBubble {
        id: floatingBubble
        visible:  !window.editing && RecordingSettings.mode === RecordingSettings.ScreenAndCamera
                 && RecordingSettings.cameraId.length > 0 && Devices.cameraAllowed
    }

    function bringToFront() {
        window.showNormal();
        window.raise();
        window.requestActivate();
    }

    // The recorder asks the window to step aside while recording, and to come back after.
    Connections {
        target: Recorder

        function onWindowHideRequested() {
            window.showMinimized();
        }

        function onWindowShowRequested() {
            window.bringToFront();
        }

        // A project recording finished (or "Edit recording" was clicked): edit it.
        function onOpenProjectRequested(folder) {
            window.openEditor(folder);
        }
    }

    // ⌘⇧R / Ctrl+Shift+R start/stop, ⌘⇧P / Ctrl+Shift+P pause ("Ctrl" means ⌘ on macOS).
    Shortcut {
        sequences: ["Ctrl+Shift+R"]
        context: Qt.ApplicationShortcut
        enabled: !window.editing || Recorder.busy
        onActivated: Recorder.toggleRecording()
    }

    Shortcut {
        sequences: ["Ctrl+Shift+P"]
        context: Qt.ApplicationShortcut
        onActivated: Recorder.togglePause()
    }

    Shortcut {
        sequences: ["Esc"]
        enabled: Recorder.state === Recorder.Countdown
        onActivated: Recorder.cancelCountdown()
    }

    // Menu-bar / system-tray controls while the window is hidden.
    Platform.SystemTrayIcon {
        visible: Recorder.busy
        icon.source: Recorder.state === Recorder.Paused ? "icons/tray-paused.png" : "icons/tray-recording.png"
        tooltip: qsTr("ScreenCam — recording %1").arg(Recorder.elapsedText)

        menu: Platform.Menu {
            Platform.MenuItem {
                text: Recorder.state === Recorder.Paused ? qsTr("Resume") : qsTr("Pause")
                enabled: Recorder.state === Recorder.Recording || Recorder.state === Recorder.Paused
                onTriggered: Recorder.togglePause()
            }

            Platform.MenuItem {
                text: qsTr("Stop recording")
                onTriggered: Recorder.stop()
            }

            Platform.MenuSeparator {}

            Platform.MenuItem {
                text: qsTr("Show ScreenCam")
                onTriggered: window.bringToFront()
            }
        }

        onActivated: (reason) => {
            if (reason === Platform.SystemTrayIcon.Trigger) {
                window.bringToFront();
            }
        }
    }
}
