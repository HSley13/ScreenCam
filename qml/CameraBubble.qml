import QtQuick

// The camera bubble on the editor's canvas: drag to move (snaps on release), drag the corner
// handle to resize. Geometry is BubbleStyle's, the same C++ the export uses.
Item {
    id: bubble

    property size canvasSize: Qt.size(1, 1)
    property var bubbleStyle: BubbleStyle  // or the editor's current segment
    property bool live: true
    // Off in the editor: a segment change is a hard cut in the video, so the preview cuts too.
    property bool animated: true
    property string placeholderText: ""
    readonly property alias video: look.video

    // QML can't see inside rectFor(): reading `revision` makes the binding update.
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

    Behavior on x { enabled: bubble.animated && !bubble.dragging; NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }
    Behavior on y { enabled: bubble.animated && !bubble.dragging; NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }
    Behavior on width { enabled: bubble.animated && !bubble.dragging; NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }
    Behavior on height { enabled: bubble.animated && !bubble.dragging; NumberAnimation { duration: Theme.animationNormal; easing.type: Easing.OutCubic } }

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

    Rectangle {  // hover ring
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

    // Resize handle, where the shape's edge crosses the bottom-right diagonal.
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

        // The centre stays put: moving the corner by d grows the bubble by 2d.
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
