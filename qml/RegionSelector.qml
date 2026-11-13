import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Drag out the region to record. Esc cancels; Enter saves it.
Window {
    id: selector

    property string screenId
    property rect screenGeometry: Qt.rect(0, 0, 800, 600)
    property real pixelRatio: 1.0

    // = the screen's logical coordinates
    property real startX: 0
    property real startY: 0
    property rect selection: Qt.rect(0, 0, 0, 0)
    readonly property bool hasSelection: selection.width >= 16 && selection.height >= 16

    x: screenGeometry.x
    y: screenGeometry.y
    width: screenGeometry.width
    height: screenGeometry.height
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    function confirm() {
        if (hasSelection) {
            RecordingSettings.screenId = screenId;
            RecordingSettings.region = Qt.rect(Math.round(selection.x), Math.round(selection.y),
                                               Math.round(selection.width), Math.round(selection.height));
            RecordingSettings.captureTarget = RecordingSettings.Region;
        }
        close();
    }

    Component.onCompleted: {
        if (RecordingSettings.screenId === screenId && RecordingSettings.region.width > 0) {
            selection = RecordingSettings.region;
        }
        requestActivate();
    }

    Shortcut {
        sequences: ["Esc"]
        onActivated: selector.close()
    }

    Shortcut {
        sequences: ["Return", "Enter"]
        onActivated: selector.confirm()
    }

    // Dim everything but the selection.
    Rectangle { x: 0; y: 0; width: parent.width; height: selector.selection.y; color: "#99000000" }
    Rectangle {
        x: 0; y: selector.selection.y + selector.selection.height
        width: parent.width; height: parent.height - y; color: "#99000000"
    }
    Rectangle {
        x: 0; y: selector.selection.y
        width: selector.selection.x; height: selector.selection.height; color: "#99000000"
    }
    Rectangle {
        x: selector.selection.x + selector.selection.width; y: selector.selection.y
        width: parent.width - x; height: selector.selection.height; color: "#99000000"
    }

    Rectangle {
        x: selector.selection.x
        y: selector.selection.y
        width: selector.selection.width
        height: selector.selection.height
        color: "transparent"
        border.color: Theme.accent
        border.width: 2
        visible: selector.selection.width > 0

        Rectangle {  // size in real pixels
            anchors.top: parent.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            radius: 6
            color: Theme.accent
            width: sizeLabel.implicitWidth + 14
            height: sizeLabel.implicitHeight + 8

            Label {
                id: sizeLabel
                anchors.centerIn: parent
                color: "white"
                font.pixelSize: 12
                text: Math.round(selector.selection.width * selector.pixelRatio) + " × "
                      + Math.round(selector.selection.height * selector.pixelRatio)
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.CrossCursor
        onPressed: (mouse) => {
            selector.startX = mouse.x;
            selector.startY = mouse.y;
            selector.selection = Qt.rect(mouse.x, mouse.y, 0, 0);
        }
        onPositionChanged: (mouse) => {
            const x = Math.max(0, Math.min(mouse.x, width));
            const y = Math.max(0, Math.min(mouse.y, height));
            selector.selection = Qt.rect(Math.min(x, selector.startX), Math.min(y, selector.startY),
                                         Math.abs(x - selector.startX), Math.abs(y - selector.startY));
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 40
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.stroke
        width: bar.implicitWidth + 32
        height: bar.implicitHeight + 20

        Row {
            id: bar
            anchors.centerIn: parent
            spacing: 12

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Drag to select the area to record")
                color: Theme.text
                font.pixelSize: 13
            }

            StyledButton {
                text: qsTr("Cancel")
                onClicked: selector.close()
            }

            StyledButton {
                accent: true
                enabled: selector.hasSelection
                text: qsTr("Use this area")
                onClicked: selector.confirm()
            }
        }
    }
}
