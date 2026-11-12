import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Small floating pill while recording (like Loom): countdown / timer, pause, stop.
// Drag it anywhere; it's excluded from the screen recording.
Window {
    id: bar

    title: qsTr("ScreenCam Controls")  // app-name prefix: hidden from the window list
    width: content.implicitWidth + 28
    height: 52
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.NoDropShadowWindowHint
           | (Qt.platform.os === "osx" ? 0 : Qt.Tool)
    visible: Recorder.busy

    onVisibleChanged: {
        if (visible) {
            // Bottom centre of the screen it appears on.
            x = Screen.virtualX + (Screen.width - width) / 2;
            y = Screen.virtualY + Screen.desktopAvailableHeight - height - 28;
            WindowTools.excludeFromCapture(bar);
        }
    }

    component BarButton: Rectangle {
        id: button

        signal clicked()
        property color tint: Theme.surfaceRaised

        implicitWidth: 32
        implicitHeight: 32
        radius: 16
        color: area.containsMouse ? Qt.lighter(tint, 1.25) : tint
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined

        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Theme.surface
        border.width: 2
        border.color: Recorder.state === Recorder.Recording ? Theme.record : Theme.stroke

        // Dragging anywhere but the buttons moves the bar.
        DragHandler {
            target: null
            onActiveChanged: {
                if (active) {
                    bar.startSystemMove();
                }
            }
        }

        Row {
            id: content
            anchors.centerIn: parent
            spacing: 12

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 10
                height: 10
                radius: 5
                color: Recorder.state === Recorder.Paused ? Theme.warning : Theme.record

                SequentialAnimation on opacity {
                    running: Recorder.state === Recorder.Recording
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.25; duration: 600 }
                    NumberAnimation { to: 1.0; duration: 600 }
                }
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                width: 118
                color: Theme.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
                text: {
                    switch (Recorder.state) {
                    case Recorder.Countdown:
                        return qsTr("Starting in %1").arg(Recorder.countdownRemaining);
                    case Recorder.Paused:
                        return qsTr("Paused %1").arg(Recorder.elapsedText);
                    case Recorder.Finalizing:
                        return qsTr("Saving…");
                    default:
                        return Recorder.elapsedText;
                    }
                }
            }

            // Pause / resume (or cancel during the countdown).
            BarButton {
                visible: Recorder.state !== Recorder.Finalizing
                onClicked: {
                    if (Recorder.state === Recorder.Countdown) {
                        Recorder.cancelCountdown();
                    } else {
                        Recorder.togglePause();
                    }
                }

                Row {
                    anchors.centerIn: parent
                    spacing: 4
                    visible: Recorder.state === Recorder.Recording
                    Rectangle { width: 4; height: 13; radius: 1; color: Theme.text }
                    Rectangle { width: 4; height: 13; radius: 1; color: Theme.text }
                }

                Text {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: Recorder.state === Recorder.Paused ? 1 : 0
                    visible: Recorder.state !== Recorder.Recording
                    text: Recorder.state === Recorder.Countdown ? "✕" : "▶"
                    color: Theme.text
                    font.pixelSize: 13
                }
            }

            // Stop: saves and opens the result.
            BarButton {
                visible: Recorder.state === Recorder.Recording || Recorder.state === Recorder.Paused
                tint: Theme.record
                onClicked: Recorder.stop()

                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 12
                    radius: 3
                    color: "white"
                }
            }
        }
    }
}
