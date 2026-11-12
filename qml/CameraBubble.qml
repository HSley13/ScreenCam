import QtQuick
import QtQuick.Effects
import QtMultimedia

// The camera bubble on top of the screen preview.
//  - Geometry comes from BubbleStyle.rectFor(), the same C++ function the recorder uses,
//    so what you see here is what lands in the video.
//  - Drag it anywhere; on release it snaps to edges/centre (BubbleStyle.snap).
//  - Drag the round handle (bottom-right, shown on hover) to resize.
Item {
    id: bubble

    // Size of the preview canvas this bubble lives on (in item pixels).
    property size canvasSize: Qt.size(1, 1)

    // `BubbleStyle.revision` is read so the binding re-runs whenever any style value changes.
    readonly property rect area: {
        BubbleStyle.revision;
        return BubbleStyle.rectFor(canvasSize.width, canvasSize.height);
    }
    readonly property real cornerRadius: {
        BubbleStyle.revision;
        return BubbleStyle.radiusFor(area.width, area.height);
    }
    readonly property real borderWidth: {
        BubbleStyle.revision;
        return BubbleStyle.borderFor(canvasSize.width, canvasSize.height);
    }
    readonly property bool dragging: moveDrag.active || resizeDrag.active

    x: area.x
    y: area.y
    width: area.width
    height: area.height

    // Shape and size changes glide; while dragging the bubble follows the pointer exactly.
    Behavior on x { enabled: !bubble.dragging; NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }
    Behavior on y { enabled: !bubble.dragging; NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }
    Behavior on width { enabled: !bubble.dragging; NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }
    Behavior on height { enabled: !bubble.dragging; NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }

    // --- Shadow: a blurred copy of the shape, offset downwards ---------------------------
    Rectangle {
        id: shadowShape
        anchors.fill: parent
        radius: bubble.cornerRadius
        color: "black"
        visible: false
    }

    MultiEffect {
        anchors.fill: shadowShape
        source: shadowShape
        visible: BubbleStyle.shadow
        shadowEnabled: true
        shadowColor: "black"
        shadowOpacity: 0.55
        shadowBlur: 1.0
        shadowVerticalOffset: Math.max(2, bubble.height * 0.03)
        autoPaddingEnabled: true
    }

    // --- Placeholder until the camera delivers frames ------------------------------------
    Rectangle {
        anchors.fill: parent
        radius: bubble.cornerRadius
        color: Theme.surfaceRaised
        visible: !Capture.cameraActive

        Text {
            anchors.centerIn: parent
            width: parent.width * 0.8
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: Capture.cameraError.length > 0 ? Capture.cameraError : qsTr("Camera starting…")
            color: Theme.textMuted
            font.pixelSize: Math.max(9, bubble.height * 0.07)
        }
    }

    // --- Camera video, clipped to the bubble shape -----------------------------------------
    VideoOutput {
        id: video
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop  // fill the shape, crop the edges
        visible: false                            // drawn through the mask effect below

        Component.onCompleted: Capture.addCameraPreview(videoSink)
        Component.onDestruction: Capture.removeCameraPreview(videoSink)
    }

    Item {
        id: mask
        anchors.fill: parent
        visible: false
        layer.enabled: true

        Rectangle {
            anchors.fill: parent
            radius: bubble.cornerRadius
            color: "black"
        }
    }

    MultiEffect {
        anchors.fill: parent
        visible: Capture.cameraActive
        source: video
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
        // Mirroring the whole masked result is fine: every shape is left/right symmetric.
        transform: Scale {
            origin.x: bubble.width / 2
            xScale: BubbleStyle.mirrored ? -1 : 1
        }
    }

    // --- Border ring --------------------------------------------------------------------
    Rectangle {
        anchors.fill: parent
        radius: bubble.cornerRadius
        color: "transparent"
        visible: bubble.borderWidth >= 0.5
        border.width: bubble.borderWidth
        border.color: BubbleStyle.borderColor
    }

    // Hover highlight so it's obvious the bubble can be grabbed.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: bubble.cornerRadius + 3
        color: "transparent"
        border.width: 2
        border.color: Theme.accent
        opacity: hover.hovered || bubble.dragging ? 0.9 : 0.0
        Behavior on opacity { NumberAnimation { duration: Theme.animationFast } }
    }

    HoverHandler {
        id: hover
        cursorShape: moveDrag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
    }

    // Move: remember where the centre was, add the pointer translation (normalized).
    DragHandler {
        id: moveDrag

        property real startX: 0
        property real startY: 0

        target: null
        onActiveChanged: {
            if (active) {
                startX = BubbleStyle.centerX;
                startY = BubbleStyle.centerY;
            } else {
                BubbleStyle.snap(bubble.canvasSize.width, bubble.canvasSize.height);
            }
        }
        onTranslationChanged: {
            if (active) {
                BubbleStyle.moveCenter(startX + translation.x / bubble.canvasSize.width,
                                       startY + translation.y / bubble.canvasSize.height,
                                       bubble.canvasSize.width, bubble.canvasSize.height);
            }
        }
    }

    // Resize handle, placed where the shape's edge crosses the bottom-right diagonal
    // (for a circle that's radius * (1 - 1/sqrt(2)) in from the bounding-box corner).
    Rectangle {
        id: handle

        readonly property real inset: bubble.cornerRadius * (1 - Math.SQRT1_2)

        width: 18
        height: 18
        radius: 9
        x: bubble.width - inset - width / 2
        y: bubble.height - inset - height / 2
        color: Theme.accent
        border.color: "white"
        border.width: 2
        opacity: hover.hovered || bubble.dragging ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: Theme.animationFast } }

        HoverHandler {
            cursorShape: Qt.SizeFDiagCursor
        }

        // The centre stays put, so moving the corner by d pixels grows the bubble by 2d.
        DragHandler {
            id: resizeDrag

            property real startSize: 0

            target: null
            onActiveChanged: {
                if (active) {
                    startSize = BubbleStyle.size;
                }
            }
            onTranslationChanged: {
                if (active) {
                    const shortSide = Math.min(bubble.canvasSize.width, bubble.canvasSize.height);
                    BubbleStyle.size = startSize + (translation.x + translation.y) / shortSide;
                }
            }
        }
    }
}
