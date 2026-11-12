import QtQuick
import QtQuick.Controls

// Thin track, purple fill, round handle that grows while dragged.
Slider {
    id: control

    implicitHeight: 26

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 4
        radius: 2
        color: "#2a2f3f"

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: Theme.accent
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.pressed ? 18 : 16
        height: width
        radius: width / 2
        color: "white"
        border.color: Theme.accent
        border.width: 2
        Behavior on width { NumberAnimation { duration: Theme.animationFast } }
    }
}
