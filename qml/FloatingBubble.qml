import QtQuick
import QtQuick.Window

// Your camera, floating on the desktop above every app (like Loom). Drag it anywhere; scroll
// over it to resize. While recording, the recording controls (timer, what's recorded, pause,
// stop) hang from its bottom edge. It is excluded from the screen recording: the camera is
// recorded as a separate track, and its place in the video is set in the editor.
Window {
    id: win

    readonly property int padding: 28  // room around the bubble for its shadow
    // Same size rules as in the video, relative to this screen.
    readonly property rect area: BubbleStyle.revision >= 0 ? BubbleStyle.rectFor(Screen.width, Screen.height)
                                                           : Qt.rect(0, 0, 0, 0)
    readonly property bool showControls: Recorder.busy
    // The controls straddle the bubble's bottom edge: half over it, half below.
    readonly property real controlsOverhang: showControls ? controls.implicitHeight / 2 : 0
    readonly property real contentWidth: Math.max(area.width, showControls ? controls.implicitWidth : 0)

    title: qsTr("ScreenCam Camera")
    width: contentWidth + 2 * padding
    height: area.height + controlsOverhang + 2 * padding

    // When the controls make the window wider, grow it equally to both sides so the bubble
    // itself doesn't jump.
    property real previousWidth: width
    onWidthChanged: {
        x = x - (width - previousWidth) / 2;
        previousWidth = width;
    }
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.NoDropShadowWindowHint
           | (Qt.platform.os === "osx" ? 0 : Qt.Tool)  // Tool windows hide with the app on macOS

    // Start in the bottom-left corner of the screen.
    Component.onCompleted: {
        x = Screen.virtualX + 40;
        y = Screen.virtualY + Screen.desktopAvailableHeight - height - 40;
    }
    onVisibleChanged: {
        if (visible) {
            WindowTools.excludeFromCapture(win);
        }
        Capture.bubbleVisible = visible;
    }
    // Report where it sits, so the editor can start with the same placement.
    onXChanged: reportTimer.restart()
    onYChanged: reportTimer.restart()

    Timer {
        id: reportTimer
        interval: 250
        onTriggered: Recorder.setBubbleDesktopCenter(Qt.point(win.x + view.x + view.width / 2,
                                                               win.y + view.y + view.height / 2))
    }

    BubbleView {
        id: view
        x: (win.width - width) / 2
        y: win.padding
        width: win.area.width
        height: win.area.height
        cornerRadius: BubbleStyle.revision >= 0 ? BubbleStyle.radiusFor(width, height) : 0
        borderWidth: BubbleStyle.revision >= 0 ? BubbleStyle.borderFor(Screen.width, Screen.height) : 0
        borderColor: BubbleStyle.borderColor
        shadowEnabled: BubbleStyle.shadow
        mirrored: BubbleStyle.mirrored
        live: Capture.cameraActive
        placeholderText: Capture.cameraError.length > 0 ? Capture.cameraError : qsTr("Camera starting…")
        feedFromCapture: true
    }

    // Accent ring on hover so it's obvious it can be grabbed.
    Rectangle {
        x: view.x - 3
        y: view.y - 3
        width: view.width + 6
        height: view.height + 6
        radius: view.cornerRadius + 3
        color: "transparent"
        border.width: 2
        border.color: Theme.accent
        opacity: hover.hovered ? 0.9 : 0.0
        Behavior on opacity { NumberAnimation { duration: Theme.animationFast } }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.OpenHandCursor
    }

    // The window manager moves the window (smooth, works across screens).
    DragHandler {
        target: null
        onActiveChanged: {
            if (active) {
                win.startSystemMove();
            }
        }
    }

    // Scroll to resize (one wheel notch = 1% of the screen's short side).
    MouseArea {
        anchors.fill: view
        acceptedButtons: Qt.NoButton
        onWheel: (wheel) => {
            BubbleStyle.size = BubbleStyle.size + wheel.angleDelta.y / 120 * 0.01;
        }
    }

    RecordingControls {
        id: controls
        visible: win.showControls
        anchors.horizontalCenter: view.horizontalCenter
        y: view.y + view.height - height / 2
        targetMaxWidth: 200
    }
}
