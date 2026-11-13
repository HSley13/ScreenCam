import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Bottom bar: record/stop button, status + timer, pause/stop or "play last recording".
Rectangle {
    id: bar

    // Emitted with the chosen .screencam folder.
    signal projectChosen(string folder)

    readonly property bool recording: Recorder.state === Recorder.Recording
    readonly property bool paused: Recorder.state === Recorder.Paused
    readonly property bool live: recording || paused
    readonly property string shortcutText: Qt.platform.os === "osx" ? "⌘⇧R" : "Ctrl+Shift+R"

    implicitHeight: 86
    color: Theme.surface
    radius: Theme.radius
    border.color: recording ? Theme.record : Theme.stroke
    border.width: recording ? 2 : 1
    Behavior on border.color { ColorAnimation { duration: Theme.animationNormal } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 18
        spacing: 16

        RecordButton {
            enabled: Recorder.state !== Recorder.Finalizing
            onClicked: Recorder.toggleRecording()
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            RowLayout {
                spacing: 8

                // Blinking dot while recording.
                Rectangle {
                    visible: bar.live
                    implicitWidth: 9
                    implicitHeight: 9
                    radius: 4.5
                    color: bar.paused ? Theme.warning : Theme.record

                    SequentialAnimation on opacity {
                        running: bar.recording
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.25; duration: 600 }
                        NumberAnimation { to: 1.0; duration: 600 }
                    }
                }

                Label {
                    text: {
                        switch (Recorder.state) {
                        case Recorder.Countdown:
                            return qsTr("Starting in %1…").arg(Recorder.countdownRemaining);
                        case Recorder.Recording:
                            return Recorder.elapsedText;
                        case Recorder.Paused:
                            return qsTr("Paused  ·  %1").arg(Recorder.elapsedText);
                        case Recorder.Finalizing:
                            return qsTr("Saving…");
                        default:
                            return qsTr("Ready to record");
                        }
                    }
                    color: Theme.text
                    font.pixelSize: bar.live ? 20 : 16
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }  // tabular digits: the timer doesn't jiggle
                }
            }

            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                color: Theme.textMuted
                font.pixelSize: 12
                text: {
                    if (bar.live) {
                        return qsTr("%1 to stop  ·  everything on the selected screen is being recorded")
                               .arg(bar.shortcutText);
                    }
                    if (Recorder.state === Recorder.Idle && Recorder.lastRecording.length > 0) {
                        return qsTr("Saved “%1”").arg(Recorder.lastRecordingName);
                    }
                    return qsTr("Press %1 or the red button to start").arg(bar.shortcutText);
                }
            }
        }

        StyledButton {
            visible: bar.live
            text: bar.paused ? qsTr("Resume") : qsTr("Pause")
            onClicked: Recorder.togglePause()
        }

        StyledButton {
            visible: bar.live
            accent: true
            tint: Theme.record
            text: qsTr("Stop")
            onClicked: Recorder.stop()
        }

        StyledButton {
            visible: Recorder.state === Recorder.Idle && Recorder.lastRecording.length > 0
            accent: true
            text: qsTr("Edit recording")
            onClicked: Recorder.openLastRecording()
        }

        // Every recording so far: reopen a project in the editor, play a video.
        StyledButton {
            id: recordingsButton
            visible: Recorder.state === Recorder.Idle
            text: qsTr("Recordings")
            onClicked: recordings.open()
        }
    }

    RecordingsPopup {
        id: recordings
        // Opens upwards, right-aligned with the bar.
        x: bar.width - width
        y: -height - 10
        onProjectChosen: (location) => bar.projectChosen(location)
    }
}
