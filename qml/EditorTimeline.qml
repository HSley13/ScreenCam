pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// One block per segment, a playhead and ticks. It only displays; seekRequested() seeks.
Item {
    id: timeline

    required property ProjectEditor editor
    property real positionMs: 0
    readonly property real durationMs: Math.max(1, editor.durationMs)
    readonly property bool scrubbing: scrubArea.pressed

    signal seekRequested(real ms)

    implicitHeight: 78

    function xFor(ms: real): real {
        return ms / timeline.durationMs * track.width;
    }

    // The smallest "nice" step that keeps labels ~80 px apart.
    readonly property real tickStepMs: {
        const steps = [500, 1000, 2000, 5000, 10000, 15000, 30000, 60000, 120000, 300000, 600000];
        const minimum = timeline.durationMs * 80 / Math.max(1, track.width);
        for (let i = 0; i < steps.length; i++) {
            if (steps[i] >= minimum) {
                return steps[i];
            }
        }
        return 1200000;
    }

    Item {
        id: ruler
        anchors.left: track.left
        anchors.right: track.right
        anchors.top: parent.top
        height: 18

        Repeater {
            model: Math.floor(timeline.durationMs / timeline.tickStepMs) + 1

            delegate: Label {
                required property int index
                x: timeline.xFor(index * timeline.tickStepMs) - (index === 0 ? 0 : width / 2)
                text: {
                    const label = timeline.editor.formatTime(index * timeline.tickStepMs);
                    return timeline.tickStepMs < 1000 ? label : label.replace(/\.\d$/, "");
                }
                color: Theme.textMuted
                font.pixelSize: 10
                font.features: { "tnum": 1 }
            }
        }
    }

    Rectangle {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: ruler.bottom
        anchors.bottom: parent.bottom
        radius: Theme.radiusSmall
        color: "#11141c"
        border.color: Theme.stroke

        Repeater {
            model: timeline.editor.segments

            delegate: Rectangle {
                id: block

                required property var modelData
                required property int index
                readonly property bool current: index === timeline.editor.currentIndex

                x: timeline.xFor(modelData.startMs) + 1
                y: 4
                width: Math.max(2, timeline.xFor(modelData.endMs - modelData.startMs) - 2)
                height: track.height - 8
                radius: 6
                color: !modelData.cameraVisible ? "#171a23" : current ? Theme.accentSoft : Theme.surfaceRaised
                border.width: current ? 2 : 1
                border.color: current ? Theme.accent : Theme.stroke
                clip: true

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    visible: block.width > 46

                    // The segment's bubble shape, or a dash when hidden.
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: block.modelData.shape === BubbleStyle.Portrait ? 9 : 14
                        height: block.modelData.shape === BubbleStyle.Landscape ? 9 : 14
                        radius: block.modelData.shape === BubbleStyle.Circle ? 7 : 3
                        color: "transparent"
                        border.width: 2
                        border.color: block.current ? Theme.accent : Theme.textMuted
                        opacity: block.modelData.cameraVisible ? 1 : 0.3
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: block.modelData.cameraVisible ? qsTr("Part %1").arg(block.index + 1) : qsTr("No camera")
                        color: block.current ? Theme.text : Theme.textMuted
                        font.pixelSize: 11
                        font.weight: block.current ? Font.DemiBold : Font.Normal
                    }
                }
            }
        }

        Rectangle {  // playhead
            x: timeline.xFor(Math.min(timeline.positionMs, timeline.durationMs)) - width / 2
            y: -6
            width: 2
            height: track.height + 6
            color: Theme.record

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: -4
                width: 10
                height: 10
                radius: 5
                color: Theme.record
            }
        }
    }

    MouseArea {  // scrub anywhere
        id: scrubArea
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        preventStealing: true

        function seekTo(mouseX: real) {
            const ratio = Math.max(0, Math.min(1, mouseX / track.width));
            timeline.seekRequested(ratio * timeline.durationMs);
        }

        onPressed: (mouse) => seekTo(mouse.x)
        onPositionChanged: (mouse) => {
            if (pressed) {
                seekTo(mouse.x);
            }
        }
    }
}
