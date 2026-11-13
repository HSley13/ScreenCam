import QtQuick
import QtQuick.Controls

// The recording pill: blinking dot, timer, what is being recorded, pause/resume and stop.
// Shown under the floating camera bubble. Never recorded: the windows that hold it are
// excluded from capture.
Rectangle {
    id: pill

    // Longest the "what is recorded" line may get before it is shortened with "…".
    property real targetMaxWidth: 220

    implicitWidth: row.implicitWidth + 24
    implicitHeight: 48
    radius: height / 2
    color: Theme.surface
    border.width: 2
    border.color: Recorder.state === Recorder.Recording ? Theme.record
                                                        : Recorder.state === Recorder.Paused ? Theme.warning : Theme.stroke

    component PillButton: Rectangle {
        id: button

        signal clicked()
        property color tint: Theme.surfaceRaised
        property string tip

        implicitWidth: 34
        implicitHeight: 34
        radius: 17
        color: area.pressed ? Qt.darker(tint, 1.2) : area.containsMouse ? Qt.lighter(tint, 1.2) : tint
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined

        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }

        ToolTip.visible: area.containsMouse && button.tip.length > 0
        ToolTip.delay: 600
        ToolTip.text: button.tip
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 10

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

        // Timer on top, what is recorded underneath.
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Label {
                color: Theme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
                text: {
                    switch (Recorder.state) {
                    case Recorder.Countdown:
                        return qsTr("Starting in %1").arg(Recorder.countdownRemaining);
                    case Recorder.Paused:
                        return qsTr("Paused · %1").arg(Recorder.elapsedText);
                    case Recorder.Finalizing:
                        return qsTr("Saving…");
                    default:
                        return Recorder.elapsedText;
                    }
                }
            }

            Label {
                width: Math.min(implicitWidth, pill.targetMaxWidth)
                elide: Text.ElideRight
                color: Theme.textMuted
                font.pixelSize: 11
                text: Capture.targetLabel
            }
        }

        // Pause / resume, or cancel during the countdown.
        PillButton {
            visible: Recorder.state !== Recorder.Finalizing
            tip: Recorder.state === Recorder.Countdown ? qsTr("Cancel")
                                                       : Recorder.state === Recorder.Paused ? qsTr("Resume") : qsTr("Pause")
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

        // Stop: saves the recording and brings ScreenCam back (editor for projects).
        PillButton {
            visible: Recorder.state === Recorder.Recording || Recorder.state === Recorder.Paused
            tint: Theme.record
            tip: qsTr("Stop and save")
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
