import QtQuick

// A camera bubble placed on a canvas (the editor's preview): geometry + drag/resize.
//  - Geometry comes from `bubbleStyle.rectFor()` — a BubbleStyle object, the same C++ geometry
//    the exporter uses — so what you see here is what lands in the video.
//  - Drag it anywhere; on release it snaps to edges/centre (BubbleStyle.snap).
//  - Drag the round handle (bottom-right, shown on hover) to resize.
Item {
    id: bubble

    // Size of the preview canvas this bubble lives on (in item pixels).
    property size canvasSize: Qt.size(1, 1)
    // Any object with BubbleStyle's API (the editor passes the current segment's style).
    property var bubbleStyle: BubbleStyle
    property bool live: true
    property string placeholderText: ""
    readonly property alias video: look.video

    // QML can't see inside C++ function calls, so a binding like rectFor(...) alone would never
    // update when the style changes. Reading bubbleStyle.revision (bumped on every change, and
    // always >= 0) makes it part of each binding's dependencies.
    readonly property rect area: bubbleStyle.revision >= 0
                                 ? bubbleStyle.rectFor(canvasSize.width, canvasSize.height)
                                 : Qt.rect(0, 0, 0, 0)
    readonly property real cornerRadius: bubbleStyle.revision >= 0 ? bubbleStyle.radiusFor(area.width, area.height) : 0
    readonly property real borderWidth: bubbleStyle.revision >= 0
                                        ? bubbleStyle.borderFor(canvasSize.width, canvasSize.height)
                                        : 0
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

    BubbleView {
        id: look
        anchors.fill: parent
        cornerRadius: bubble.cornerRadius
        borderWidth: bubble.borderWidth
        borderColor: bubble.bubbleStyle.borderColor
        shadowEnabled: bubble.bubbleStyle.shadow
        mirrored: bubble.bubbleStyle.mirrored
        live: bubble.live
        placeholderText: bubble.placeholderText
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
                startX = bubble.bubbleStyle.centerX;
                startY = bubble.bubbleStyle.centerY;
            } else {
                bubble.bubbleStyle.snap(bubble.canvasSize.width, bubble.canvasSize.height);
            }
        }
        onTranslationChanged: {
            if (active) {
                bubble.bubbleStyle.moveCenter(startX + translation.x / bubble.canvasSize.width,
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
                    startSize = bubble.bubbleStyle.size;
                }
            }
            onTranslationChanged: {
                if (active) {
                    const shortSide = Math.min(bubble.canvasSize.width, bubble.canvasSize.height);
                    bubble.bubbleStyle.size = startSize + (translation.x + translation.y) / shortSide;
                }
            }
        }
    }
}
