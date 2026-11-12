import QtQuick
import QtQuick.Controls
import QtMultimedia

// The big preview on the left: a canvas with the exact aspect ratio of the output video,
// showing the live screen (and the camera bubble on top), or just the camera.
Rectangle {
    id: stage

    readonly property bool cameraOnly: RecordingSettings.mode === RecordingSettings.CameraOnly
    readonly property bool showBubble: RecordingSettings.mode === RecordingSettings.ScreenAndCamera
                                       && RecordingSettings.cameraId.length > 0
    readonly property real canvasAspect: {
        const out = Capture.outputSize;
        if (out.width > 0 && out.height > 0) {
            return out.width / out.height;
        }
        if (cameraOnly) {
            return RecordingSettings.cameraOnlyOrientation === RecordingSettings.Portrait ? 9 / 16 : 16 / 9;
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

            VideoOutput {
                id: screenView
                anchors.fill: parent
                visible: !stage.cameraOnly
                fillMode: VideoOutput.Stretch  // the canvas already has the screen's aspect ratio

                Component.onCompleted: Capture.addScreenPreview(videoSink)
                Component.onDestruction: Capture.removeScreenPreview(videoSink)
            }

            VideoOutput {
                id: cameraView
                anchors.fill: parent
                visible: stage.cameraOnly
                fillMode: VideoOutput.PreserveAspectCrop
                transform: Scale {
                    origin.x: cameraView.width / 2
                    xScale: BubbleStyle.mirrored ? -1 : 1
                }

                Component.onCompleted: Capture.addCameraPreview(videoSink)
                Component.onDestruction: Capture.removeCameraPreview(videoSink)
            }

            // Shown until frames arrive, or with the reason they don't.
            Column {
                anchors.centerIn: parent
                width: parent.width * 0.7
                spacing: 6
                visible: stage.cameraOnly ? !Capture.cameraActive : !Capture.screenActive

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: {
                        if (stage.cameraOnly) {
                            if (RecordingSettings.cameraId.length === 0) {
                                return qsTr("Pick a camera in the sidebar");
                            }
                            return Capture.cameraError.length > 0 ? Capture.cameraError : qsTr("Starting camera…");
                        }
                        return Capture.screenError.length > 0 ? Capture.screenError : qsTr("Starting screen preview…");
                    }
                    color: Theme.textMuted
                    font.pixelSize: 14
                }

            }

            // Safe-area outline + centre lines while dragging: the snap targets.
            Rectangle {
                readonly property real margin: Math.min(canvas.width, canvas.height) * 0.03

                x: margin
                y: margin
                width: canvas.width - 2 * margin
                height: canvas.height - 2 * margin
                color: "transparent"
                border.color: Theme.accent
                border.width: 1
                opacity: bubble.dragging ? 0.6 : 0.0
                Behavior on opacity { NumberAnimation { duration: Theme.animationFast } }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 1
                    height: parent.height
                    color: Theme.accent
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 1
                    color: Theme.accent
                }
            }

            CameraBubble {
                id: bubble
                visible: stage.showBubble
                canvasSize: stage.canvasSize
            }
        }
    }
}
