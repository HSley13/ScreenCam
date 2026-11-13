import QtQuick
import QtQuick.Controls
import QtMultimedia

// The big preview on the left: a canvas with the exact aspect ratio of the output video,
// showing the live screen. (Your camera floats on the desktop; you place it in the editor.)
Rectangle {
    id: stage

    readonly property real canvasAspect: {
        const out = Capture.outputSize;
        if (out.width > 0 && out.height > 0) {
            return out.width / out.height;
        }
        return 16 / 9;
    }
    readonly property size canvasSize: Qt.size(canvas.width, canvas.height)

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.stroke

    // --- Header: title + what will be recorded --------------------------------------------
    Row {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 18
        spacing: 10

        Label {
            text: qsTr("Preview")
            color: Theme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Your camera floats on the desktop — place it exactly in the editor afterwards")
            color: Theme.textMuted
            font.pixelSize: 11
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            radius: height / 2
            color: Theme.accentSoft
            height: 24
            width: info.implicitWidth + 20
            visible: Capture.outputSize.width > 0

            Label {
                id: info
                anchors.centerIn: parent
                text: Capture.outputSize.width + " × " + Capture.outputSize.height + "  ·  "
                      + RecordingSettings.frameRate + " fps  ·  MP4"
                color: Theme.text
                font.pixelSize: 11
                font.weight: Font.Medium
            }
        }
    }

    // --- Canvas, letter-boxed inside the stage ---------------------------------------------
    Item {
        id: area
        anchors.fill: parent
        anchors.margins: 18
        anchors.topMargin: 56

        Rectangle {
            id: canvas

            readonly property real fitWidth: Math.min(area.width, area.height * stage.canvasAspect)

            anchors.centerIn: parent
            width: fitWidth
            height: fitWidth / stage.canvasAspect
            color: "#07080b"
            clip: true
            Behavior on width { NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }
            Behavior on height { NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }

            // For a region, the full screen is drawn larger than the canvas and shifted so only
            // the selected part shows (the canvas clips the rest).
            VideoOutput {
                id: screenView
                readonly property rect crop: Capture.sourceCrop
                x: crop.width > 0 ? -crop.x / crop.width * canvas.width : 0
                y: crop.height > 0 ? -crop.y / crop.height * canvas.height : 0
                width: crop.width > 0 ? canvas.width / crop.width : canvas.width
                height: crop.height > 0 ? canvas.height / crop.height : canvas.height
                fillMode: RecordingSettings.captureTarget === RecordingSettings.Window
                          ? VideoOutput.PreserveAspectFit : VideoOutput.Stretch

                Component.onCompleted: Capture.addScreenPreview(videoSink)
                Component.onDestruction: Capture.removeScreenPreview(videoSink)
            }

            // Shown until frames arrive, or with the reason they don't.
            Column {
                anchors.centerIn: parent
                width: parent.width * 0.7
                spacing: 6
                visible: !Capture.screenActive

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: Capture.screenError.length > 0 ? Capture.screenError : qsTr("Starting screen preview…")
                    color: Theme.textMuted
                    font.pixelSize: 14
                }

            }

        }
    }
}
