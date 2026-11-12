import QtQuick
import QtQuick.Controls

// Label on the left, pill toggle on the right.
Switch {
    id: control

    implicitHeight: 30
    font.pixelSize: 13

    indicator: Rectangle {
        implicitWidth: 38
        implicitHeight: 22
        x: control.width - width - control.rightPadding
        y: (control.height - height) / 2
        radius: height / 2
        color: control.checked ? Theme.accent : "#2a2f3f"
        Behavior on color { ColorAnimation { duration: Theme.animationFast } }

        Rectangle {
            width: 16
            height: 16
            radius: 8
            y: 3
            x: control.checked ? parent.width - width - 3 : 3
            color: "white"
            Behavior on x { NumberAnimation { duration: Theme.animationFast; easing.type: Easing.OutCubic } }
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? Theme.text : Theme.textMuted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        rightPadding: control.indicator.width + 10
    }
}
