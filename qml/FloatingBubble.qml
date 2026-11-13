import QtQuick
import QtQuick.Window

// Your camera, floating above every app: drag it, scroll to resize. While recording, the
// controls hang from it. Excluded from the screen recording (the camera is its own track).
Window {
    id: win

    readonly property int padding: 28  // for the shadow
    // Sized relative to the recorded screen, not the one it's on: resizing mid-drag onto
    // another monitor made the window manager throw it back.
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
    // Half over the bubble's bottom edge, half below.
    readonly property real controlsOverhang: showControls ? controls.implicitHeight / 2 : 0
    readonly property real contentWidth: Math.max(area.width, showControls ? controls.implicitWidth : 0)

    title: qsTr("ScreenCam Camera")
    width: contentWidth + 2 * padding
    height: area.height + controlsOverhang + 2 * padding

    // Grow both ways so the bubble doesn't jump; never mid-drag (it fights the window manager).
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

    function placeOnRecordedScreen() {
        x = referenceGeometry.x + 40;
        y = referenceGeometry.y + referenceGeometry.height - height - 90;  // above a bottom Dock/taskbar
    }
    Component.onCompleted: placeOnRecordedScreen()

    // Follows the recorded screen, except during a recording.
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
    // So the editor starts with the same placement.
    onXChanged: {
        reportTimer.restart();
        settleTimer.restart();
    }
    onYChanged: {
        reportTimer.restart();
        settleTimer.restart();
    }

    // A window-manager drag has no "finished" signal: wait until the window stops moving.
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

    Rectangle {  // hover ring
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

    // The window manager moves the window (free to cross monitors); else we move it ourselves.
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
                // scenePosition is relative to the moving window.
                win.x = pressWindow.x + (win.x + centroid.scenePosition.x - pressPointer.x);
                win.y = pressWindow.y + (win.y + centroid.scenePosition.y - pressPointer.y);
            }
        }
    }

    // One wheel notch = 1% of the recorded screen's short side.
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
