import QtQuick
import QtQuick.Window

// Your camera, floating on the desktop above every app (like Loom). Drag it anywhere; scroll
// over it to resize. While recording, the recording controls (timer, what's recorded, pause,
// stop) hang from its bottom edge. It is excluded from the screen recording: the camera is
// recorded as a separate track, and its place in the video is set in the editor.
Window {
    id: win

    readonly property int padding: 28  // room around the bubble for its shadow
    // The bubble is sized like it will be in the video: relative to the *recorded* screen.
    // Not relative to the screen the bubble happens to be on — then dragging it onto another
    // monitor resized it mid-drag, and the window manager threw it back where it came from.
    readonly property rect referenceGeometry: {
        const screens = Devices.screens;
        for (let i = 0; i < screens.length; i++) {
            if (screens[i].id === RecordingSettings.screenId && screens[i].geometry !== undefined) {
                return screens[i].geometry;
            }
        }
        return Qt.rect(Screen.virtualX, Screen.virtualY, Screen.width, Screen.height);
    }
    readonly property size referenceSize: Qt.size(referenceGeometry.width, referenceGeometry.height)
    readonly property rect area: BubbleStyle.revision >= 0
                                 ? BubbleStyle.rectFor(referenceSize.width, referenceSize.height)
                                 : Qt.rect(0, 0, 0, 0)
    readonly property bool showControls: Recorder.busy
    // The controls straddle the bubble's bottom edge: half over it, half below.
    readonly property real controlsOverhang: showControls ? controls.implicitHeight / 2 : 0
    readonly property real contentWidth: Math.max(area.width, showControls ? controls.implicitWidth : 0)

    title: qsTr("ScreenCam Camera")
    width: contentWidth + 2 * padding
    height: area.height + controlsOverhang + 2 * padding

    // When the controls make the window wider, grow it equally to both sides so the bubble
    // itself doesn't jump. Never while it's being dragged: moving a window from code during a
    // drag fights the window manager.
    property real previousWidth: width
    property bool moving: false
    onWidthChanged: {
        if (!moving) {
            x = x - (width - previousWidth) / 2;
        }
        previousWidth = width;
    }
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.NoDropShadowWindowHint
           | (Qt.platform.os === "osx" ? 0 : Qt.Tool)  // Tool windows hide with the app on macOS

    // Start in the bottom-left corner of the recorded screen.
    function placeOnRecordedScreen() {
        x = referenceGeometry.x + 40;
        y = referenceGeometry.y + referenceGeometry.height - height - 90;  // above a bottom Dock/taskbar
    }
    Component.onCompleted: placeOnRecordedScreen()

    // Picking another screen to record (before recording) brings the bubble along to it.
    // During a recording it stays wherever you put it.
    Connections {
        target: RecordingSettings
        function onScreenIdChanged() {
            if (!Recorder.busy) {
                win.placeOnRecordedScreen();
            }
        }
    }
    onVisibleChanged: {
        if (visible) {
            WindowTools.excludeFromCapture(win);
        }
        Capture.bubbleVisible = visible;
    }
    // Report where it sits, so the editor can start with the same placement.
    onXChanged: {
        reportTimer.restart();
        settleTimer.restart();
    }
    onYChanged: {
        reportTimer.restart();
        settleTimer.restart();
    }

    // A drag handed to the window manager has no "finished" signal: call it finished once the
    // window has stopped moving for a moment.
    Timer {
        id: settleTimer
        interval: 400
        onTriggered: win.moving = false
    }

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
        borderWidth: BubbleStyle.revision >= 0 ? BubbleStyle.borderFor(win.referenceSize.width, win.referenceSize.height) : 0
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

    // The window manager moves the window (smooth, and free to cross onto other monitors).
    // If the platform can't do that, move it ourselves: window position + how far the
    // pointer has travelled on the desktop since the press.
    DragHandler {
        id: drag

        property bool manual: false
        property point pressWindow
        property point pressPointer

        target: null
        onActiveChanged: {
            if (active) {
                win.moving = true;
                manual = !win.startSystemMove();
                pressWindow = Qt.point(win.x, win.y);
                pressPointer = Qt.point(win.x + centroid.scenePosition.x, win.y + centroid.scenePosition.y);
            } else {
                manual = false;
            }
        }
        onCentroidChanged: {
            if (active && manual) {
                // scenePosition is relative to the window, which moves: add its position back.
                win.x = pressWindow.x + (win.x + centroid.scenePosition.x - pressPointer.x);
                win.y = pressWindow.y + (win.y + centroid.scenePosition.y - pressPointer.y);
            }
        }
    }

    // Scroll to resize (one wheel notch = 1% of the recorded screen's short side).
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
