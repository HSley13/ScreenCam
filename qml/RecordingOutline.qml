import QtQuick
import QtQuick.Controls
import QtQuick.Window

// A coloured frame around the window or region being recorded, with a tag naming it — so
// you always know what's in the video. Clicks go straight through it, and it's excluded
// from the recording. The recorded window can move: its place is re-read a few times per
// second. Not shown for a whole screen (the frame would just be the screen's edge).
Window {
    id: outline

    readonly property int margin: 4       // frame sits just outside the recorded area
    readonly property int stroke: 3
    readonly property int tagHeight: 26
    property rect target: Qt.rect(0, 0, 0, 0)
    readonly property bool hasTarget: target.width > 0 && target.height > 0
    // Tag above the frame, unless that would leave the top of the screen.
    readonly property bool tagInside: target.y - tagHeight - margin < Screen.virtualY + 4
    readonly property color frameColor: Recorder.state === Recorder.Paused ? Theme.warning
                                      : Recorder.state === Recorder.Countdown ? Theme.accent : Theme.record

    title: qsTr("ScreenCam Outline")
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.NoDropShadowWindowHint
           | Qt.WindowTransparentForInput | Qt.WindowDoesNotAcceptFocus
           | (Qt.platform.os === "osx" ? 0 : Qt.Tool)
    visible: Recorder.busy && Recorder.state !== Recorder.Finalizing && hasTarget

    x: target.x - margin
    y: target.y - margin - (tagInside ? 0 : tagHeight)
    width: target.width + 2 * margin
    height: target.height + 2 * margin + (tagInside ? 0 : tagHeight)

    onVisibleChanged: {
        if (visible) {
            WindowTools.excludeFromCapture(outline);
        }
    }

    function refresh() {
        const bounds = Capture.targetBounds();
        if (bounds.x !== target.x || bounds.y !== target.y || bounds.width !== target.width
                || bounds.height !== target.height) {
            target = bounds;
        }
    }

    Timer {
        interval: 250
        repeat: true
        triggeredOnStart: true
        running: Recorder.busy
        onTriggered: outline.refresh()
    }

    // Also follow choices made while recording is about to start (countdown).
    Connections {
        target: Capture
        function onTargetChanged() {
            outline.refresh();
        }
    }

    Rectangle {
        id: frame
        x: 0
        y: outline.tagInside ? 0 : outline.tagHeight
        width: outline.width
        height: outline.height - y
        color: "transparent"
        radius: 8
        border.width: outline.stroke
        border.color: outline.frameColor
    }

    // "● Recording · Safari — GitHub"
    Rectangle {
        x: outline.tagInside ? outline.margin + 8 : 0
        y: outline.tagInside ? outline.margin + 8 : 0
        width: Math.min(tagText.implicitWidth + 20, outline.width)
        height: outline.tagHeight - 4
        radius: height / 2
        color: outline.frameColor

        Label {
            id: tagText
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: "white"
            font.pixelSize: 12
            font.weight: Font.DemiBold
            text: {
                const what = Capture.targetLabel;
                switch (Recorder.state) {
                case Recorder.Countdown:
                    return qsTr("Will record · %1").arg(what);
                case Recorder.Paused:
                    return qsTr("Paused · %1").arg(what);
                default:
                    return qsTr("● Recording · %1").arg(what);
                }
            }
        }
    }
}
