import QtQuick
import QtQuick.Effects
import QtMultimedia

// The bubble's look (shape, video, border, shadow) without behaviour.
Item {
    id: view

    property real cornerRadius: Math.min(width, height) / 2
    property real borderWidth: 0
    property color borderColor: "white"
    property bool shadowEnabled: true
    property bool mirrored: true
    property bool live: false          // false: the placeholder
    property string placeholderText: ""
    // false: someone else feeds `video` (the editor's MediaPlayer).
    property bool feedFromCapture: false
    readonly property alias video: video

    Rectangle {  // shadow
        id: shadowShape
        anchors.fill: parent
        radius: view.cornerRadius
        color: "black"
        visible: false
    }

    MultiEffect {
        anchors.fill: shadowShape
        source: shadowShape
        visible: view.shadowEnabled
        shadowEnabled: true
        shadowColor: "black"
        shadowOpacity: 0.55
        shadowBlur: 1.0
        shadowVerticalOffset: Math.max(2, view.height * 0.03)
        autoPaddingEnabled: true
    }

    Rectangle {  // placeholder
        anchors.fill: parent
        radius: view.cornerRadius
        color: Theme.surfaceRaised
        visible: !view.live

        Text {
            anchors.centerIn: parent
            width: parent.width * 0.8
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: view.placeholderText
            color: Theme.textMuted
            font.pixelSize: Math.max(9, view.height * 0.07)
        }
    }

    VideoOutput {
        id: video
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
        visible: false  // drawn through the mask below

        Component.onCompleted: {
            if (view.feedFromCapture) {
                Capture.addCameraPreview(videoSink);
            }
        }
        Component.onDestruction: {
            if (view.feedFromCapture) {
                Capture.removeCameraPreview(videoSink);
            }
        }
    }

    Item {
        id: mask
        anchors.fill: parent
        visible: false
        layer.enabled: true

        Rectangle {
            anchors.fill: parent
            radius: view.cornerRadius
            color: "black"
        }
    }

    MultiEffect {
        anchors.fill: parent
        visible: view.live
        source: video
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
        // Every shape is left/right symmetric, so mirroring after the mask is fine.
        transform: Scale {
            origin.x: view.width / 2
            xScale: view.mirrored ? -1 : 1
        }
    }

    Rectangle {  // border
        anchors.fill: parent
        radius: view.cornerRadius
        color: "transparent"
        visible: view.borderWidth >= 0.5
        border.width: view.borderWidth
        border.color: view.borderColor
    }
}
