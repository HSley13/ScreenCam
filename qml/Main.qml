import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

        PreviewStage {
            id: stage
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        SettingsPanel {
            Layout.preferredWidth: Theme.sidebarWidth
            Layout.fillHeight: true

            BubbleSection {
                canvasSize: stage.canvasSize
            }
        }
    }
}
