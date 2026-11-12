pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// Four tiles, each drawing a miniature of the bubble shape it selects.
RowLayout {
    id: root

    property int shape: BubbleStyle.Circle
    signal picked(int shape)

    spacing: 8

    Repeater {
        model: [
            { shape: BubbleStyle.Circle, label: qsTr("Circle"), w: 26, h: 26, round: 1.0 },
            { shape: BubbleStyle.Squircle, label: qsTr("Squircle"), w: 26, h: 26, round: 0.4 },
            { shape: BubbleStyle.Portrait, label: qsTr("Portrait"), w: 18, h: 30, round: 0.3 },
            { shape: BubbleStyle.Landscape, label: qsTr("Landscape"), w: 32, h: 18, round: 0.3 }
        ]

        delegate: Rectangle {
            id: tile

            required property var modelData
            readonly property bool selected: root.shape === modelData.shape

            Layout.fillWidth: true
            implicitHeight: 66
            radius: Theme.radiusSmall
            color: selected ? Theme.accentSoft : (hover.containsMouse ? Theme.surfaceRaised : "#181c27")
            border.color: selected ? Theme.accent : Theme.stroke
            border.width: selected ? 2 : 1
            Behavior on color { ColorAnimation { duration: Theme.animationFast } }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 10
                width: tile.modelData.w
                height: tile.modelData.h
                radius: Math.min(width, height) / 2 * tile.modelData.round
                color: tile.selected ? Theme.accent : Theme.textMuted
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 7
                text: tile.modelData.label
                color: tile.selected ? Theme.text : Theme.textMuted
                font.pixelSize: 10
            }

            MouseArea {
                id: hover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.picked(tile.modelData.shape)
            }
        }
    }
}
