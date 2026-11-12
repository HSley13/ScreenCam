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

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // Preview stage (filled in by the live-preview feature).
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.stroke

            Label {
                anchors.centerIn: parent
                text: qsTr("ScreenCam")
                color: Theme.textMuted
                font.pixelSize: 22
            }
        }

        SettingsPanel {
            Layout.preferredWidth: Theme.sidebarWidth
            Layout.fillHeight: true
        }
    }
}
