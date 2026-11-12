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

    // macOS asks for camera/microphone access the first time; elsewhere this is a no-op.
    Component.onCompleted: Devices.requestPermissions()

    // Run the live inputs only while the window can actually be seen.
    Binding {
        target: Capture
        property: "previewEnabled"
        value: window.visible && window.visibility !== Window.Minimized
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

    // Your camera, floating on the desktop (only in Screen + Camera mode).
    FloatingBubble {
        visible: RecordingSettings.mode === RecordingSettings.ScreenAndCamera
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
    }

    // ⌘⇧R / Ctrl+Shift+R start/stop, ⌘⇧P / Ctrl+Shift+P pause ("Ctrl" means ⌘ on macOS).
    Shortcut {
        sequences: ["Ctrl+Shift+R"]
        context: Qt.ApplicationShortcut
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
