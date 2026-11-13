import QtQuick

// The big record button: a red dot that morphs into a rounded "stop" square while
// recording, with a pulsing halo.
Item {
    id: root

    signal clicked()

    readonly property bool live: Recorder.state === Recorder.Recording || Recorder.state === Recorder.Paused

    implicitWidth: 58
    implicitHeight: 58
    scale: mouse.pressed ? 0.93 : 1.0
    opacity: enabled ? 1.0 : 0.5
    Behavior on scale { NumberAnimation { duration: Theme.animationFast } }

    Rectangle {  // pulsing halo
        id: halo
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        radius: width / 2
        color: "transparent"
        border.color: Theme.record
        border.width: 2
        visible: Recorder.state === Recorder.Recording

        ParallelAnimation {
            running: halo.visible
            loops: Animation.Infinite
            NumberAnimation { target: halo; property: "scale"; from: 1.0; to: 1.45; duration: 1300; easing.type: Easing.OutCubic }
            NumberAnimation { target: halo; property: "opacity"; from: 0.8; to: 0.0; duration: 1300 }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: 3
        border.color: mouse.containsMouse ? "white" : Qt.rgba(1, 1, 1, 0.7)
        Behavior on border.color { ColorAnimation { duration: Theme.animationFast } }
    }

    Rectangle {
        anchors.centerIn: parent
        width: root.live ? 22 : 42
        height: width
        radius: root.live ? 6 : width / 2
        color: Theme.record
        Behavior on width { NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutBack } }
        Behavior on radius { NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
