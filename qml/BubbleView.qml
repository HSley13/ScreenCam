import QtQuick
import QtQuick.Effects
import QtMultimedia

// What the camera bubble looks like — shape, video, border, shadow — with no behaviour.
// Used by the floating desktop bubble and by the editor's preview.
Item {
    id: view

    property real cornerRadius: Math.min(width, height) / 2
    property real borderWidth: 0
    property color borderColor: "white"
    property bool shadowEnabled: true
    property bool mirrored: true
    property bool live: false          // false: show the placeholder instead of video
    property string placeholderText: ""
    // true: register the video with the live camera (Capture); false: someone else feeds it
    // (the editor attaches a MediaPlayer to `video`).
    property bool feedFromCapture: false
    readonly property alias video: video

    // --- Shadow: a blurred copy of the shape, offset downwards ------------------------------
    Rectangle {
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

    // --- Placeholder until there is video --------------------------------------------------
    Rectangle {
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

    // --- Video, clipped to the shape -----------------------------------------------------------
    VideoOutput {
        id: video
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop  // fill the shape, crop the edges
        visible: false                            // drawn through the mask effect below

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
        // Mirroring the masked result is fine: every shape is left/right symmetric.
        transform: Scale {
            origin.x: view.width / 2
            xScale: view.mirrored ? -1 : 1
        }
    }

    // --- Border ring --------------------------------------------------------------------------
    Rectangle {
        anchors.fill: parent
        radius: view.cornerRadius
        color: "transparent"
        visible: view.borderWidth >= 0.5
        border.width: view.borderWidth
        border.color: view.borderColor
    }
}
