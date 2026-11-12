import QtQuick
import QtQuick.Controls

// Flat button; `accent: true` gives the filled purple variant.
Button {
    id: control

    property bool accent: false
    property color tint: Theme.accent

    implicitHeight: 34
    leftPadding: 14
    rightPadding: 14
    font.pixelSize: 13
    font.weight: Font.DemiBold
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.accent ? "white" : (control.enabled ? Theme.text : Theme.textMuted)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.accent
               ? (control.down ? Qt.darker(control.tint, 1.2) : control.hovered ? Qt.lighter(control.tint, 1.1) : control.tint)
               : (control.down ? "#262b3a" : control.hovered ? Theme.surfaceRaised : "#181c27")
        border.color: control.accent ? "transparent" : Theme.stroke
        opacity: control.enabled ? 1.0 : 0.5
        Behavior on color { ColorAnimation { duration: Theme.animationFast } }
    }
}
