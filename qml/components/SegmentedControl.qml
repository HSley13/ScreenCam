pragma ComponentBehavior: Bound

import QtQuick

// A row of mutually exclusive options with a sliding highlight.
//  options: [{ value: <any>, label: "Text" }, ...]
//  value:   the selected option's value; activated(value) fires on click.
Rectangle {
    id: root

    property var options: []
    property var value
    signal activated(var value)

    readonly property int currentIndex: {
        for (let i = 0; i < options.length; i++) {
            if (options[i].value === value) {
                return i;
            }
        }
        return -1;
    }
    readonly property real segmentWidth: (width - 6) / Math.max(1, options.length)

    implicitHeight: 36
    radius: Theme.radiusSmall
    color: "#181c27"
    border.color: Theme.stroke
    opacity: enabled ? 1.0 : 0.5

    Rectangle {
        visible: root.currentIndex >= 0
        x: 3 + root.currentIndex * root.segmentWidth
        y: 3
        width: root.segmentWidth
        height: root.height - 6
        radius: Theme.radiusSmall - 3
        color: Theme.accent
        Behavior on x { NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }
    }

    Row {
        anchors.fill: parent
        anchors.margins: 3

        Repeater {
            model: root.options

            delegate: Item {
                id: segment

                required property var modelData
                required property int index

                width: root.segmentWidth
                height: parent.height

                Text {
                    anchors.centerIn: parent
                    text: segment.modelData.label
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: segment.index === root.currentIndex ? "white" : (hover.containsMouse ? Theme.text : Theme.textMuted)
                    Behavior on color { ColorAnimation { duration: Theme.animationFast } }
                }

                MouseArea {
                    id: hover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.activated(segment.modelData.value)
                }
            }
        }
    }
}
