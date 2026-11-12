import QtQuick
import QtQuick.Window

// Your camera, floating on the desktop above every app (like Loom). Drag it anywhere; scroll
// over it to resize. It is excluded from the screen recording: the camera is recorded as a
// separate track, and its place in the video is set (and can be changed) in the editor.
Window {
    id: win

    readonly property int padding: 28  // room around the bubble for its shadow
    // Same size rules as in the video, relative to this screen.
    readonly property rect area: BubbleStyle.revision >= 0 ? BubbleStyle.rectFor(Screen.width, Screen.height)
                                                           : Qt.rect(0, 0, 0, 0)

    title: qsTr("ScreenCam Camera")  // starts with the app name: hidden from the window list
    width: area.width + 2 * padding
    height: area.height + 2 * padding
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
        onTriggered: Recorder.setBubbleDesktopCenter(Qt.point(win.x + win.width / 2, win.y + win.height / 2))
    }

    BubbleView {
        id: view
        x: win.padding
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
}
