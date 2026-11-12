import QtQuick
import QtQuick.Window

// The recording controls on their own, floating at the bottom of the screen — used when
// there is no camera bubble to attach them to (Screen-only recordings). Drag it anywhere;
// it's excluded from the screen recording.
Window {
    id: bar

    // The floating camera bubble carries the same controls; no need for two.
    property bool bubbleShown: false

    title: qsTr("ScreenCam Controls")
    width: controls.implicitWidth + 16
    height: controls.implicitHeight + 16
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.NoDropShadowWindowHint
           | (Qt.platform.os === "osx" ? 0 : Qt.Tool)
    visible: Recorder.busy && !bubbleShown

    onVisibleChanged: {
        if (visible) {
            // Bottom centre of the screen it appears on.
            x = Screen.virtualX + (Screen.width - width) / 2;
            y = Screen.virtualY + Screen.desktopAvailableHeight - height - 28;
            WindowTools.excludeFromCapture(bar);
        }
    }

    RecordingControls {
        id: controls
        anchors.centerIn: parent

        // Dragging anywhere but the buttons moves the bar.
        DragHandler {
            target: null
            onActiveChanged: {
                if (active) {
                    bar.startSystemMove();
                }
            }
        }
    }
}
