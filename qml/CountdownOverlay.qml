import QtQuick
import QtQuick.Controls

// Full-window 3-2-1 countdown. Clicks are swallowed; Esc or "Cancel" aborts.
Rectangle {
    id: overlay

    color: Qt.rgba(0.03, 0.04, 0.06, 0.82)
    opacity: Recorder.state === Recorder.Countdown ? 1.0 : 0.0
    visible: opacity > 0.0
    Behavior on opacity { NumberAnimation { duration: Theme.animationNormal } }

    MouseArea {
        anchors.fill: parent
    }

    Text {
        id: number
        anchors.centerIn: parent
        text: Recorder.countdownRemaining
        color: "white"
        font.pixelSize: 168
        font.weight: Font.Bold

        // Each new number pops in.
        onTextChanged: pop.restart()

        ParallelAnimation {
            id: pop
            NumberAnimation { target: number; property: "scale"; from: 1.5; to: 1.0; duration: 420; easing.type: Easing.OutBack }
            NumberAnimation { target: number; property: "opacity"; from: 0.0; to: 1.0; duration: 250 }
        }
    }

    Column {
        anchors.top: number.bottom
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 14

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: RecordingSettings.hideWindowWhileRecording ? qsTr("ScreenCam will hide when recording starts")
                                                             : qsTr("Recording starts soon")
            color: Theme.textMuted
            font.pixelSize: 14
        }

        StyledButton {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Cancel  (Esc)")
            onClicked: Recorder.cancelCountdown()
        }
    }
}
