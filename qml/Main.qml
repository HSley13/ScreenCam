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

    property string initialProject: ""  // `--open`, or a Finder double-click
    // By project folder; also keeps them from being garbage-collected.
    property var editorWindows: ({})
    // While an editor is active the live inputs pause and the floating bubble steps aside.
    property Window activeEditor: null
    readonly property bool editing: activeEditor !== null

    Component.onCompleted: {
        Devices.requestPermissions();
        if (initialProject.length > 0) {
            openEditor(initialProject);
        }
    }

    function openEditor(folder: string) {
        const key = folder.replace(/^file:\/\//, "").replace(/\/(project\.json)?$/, "");
        const existing = window.editorWindows[key];
        if (existing) {
            existing.bringToFront();
            return;
        }
        const editor = editorComponent.createObject(window, { folder: folder });
        window.editorWindows[key] = editor;
        editor.activeChanged.connect(() => {
            if (editor.active) {
                window.activeEditor = editor;
            } else if (window.activeEditor === editor) {
                window.activeEditor = null;
            }
        });
        editor.finished.connect(() => {
            if (window.activeEditor === editor) {
                window.activeEditor = null;
            }
            delete window.editorWindows[key];
            editor.destroy();
        });
        editor.bringToFront();
    }

    // Hot reload (debug builds) reopens these after rebuilding the UI.
    function openProjects() {
        return Object.keys(window.editorWindows);
    }

    Component {
        id: editorComponent
        EditorWindow {}
    }

    // Closing the recorder quits ScreenCam (editors save as they close), except mid-recording.
    onClosing: (close) => {
        if (Recorder.busy) {
            close.accepted = false;
            window.showMinimized();
            return;
        }
        Qt.quit();
    }
    // Re-applied whenever the window is shown again.
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

    RowLayout {
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

    CountdownOverlay {
        anchors.fill: parent
    }

    TargetTag {}

    // Steps aside for an editor, but never mid-recording: it carries the Stop button.
    FloatingBubble {
        visible: RecordingSettings.cameraId.length > 0 && Devices.cameraAllowed
                 && (Recorder.busy || !window.editing)
    }

    function bringToFront() {
        window.showNormal();
        window.raise();
        window.requestActivate();
    }

    Connections {
        target: Recorder

        function onWindowHideRequested() {
            window.showMinimized();
        }

        function onWindowShowRequested() {
            window.bringToFront();
        }

        function onOpenProjectRequested(folder) {
            window.openEditor(folder);
        }
    }

    // "Ctrl" is ⌘ on macOS.
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
