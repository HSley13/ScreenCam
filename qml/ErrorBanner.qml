import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Red banner: the last recording error (dismissible), or a setup problem that blocks
// recording altogether (e.g. Qt without its FFmpeg backend), which stays until fixed.
Rectangle {
    id: banner

    readonly property bool recorderError: Recorder.errorString.length > 0
    readonly property string message: recorderError ? Recorder.errorString : Capture.backendProblem

    implicitHeight: visible ? row.implicitHeight + 20 : 0
    visible: message.length > 0
    radius: Theme.radiusSmall
    color: Qt.rgba(1.0, 0.30, 0.37, 0.14)
    border.color: Theme.record

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: 10
        anchors.leftMargin: 14
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: banner.message
            color: Theme.text
            wrapMode: Text.WordWrap
            font.pixelSize: 13
        }

        StyledButton {
            visible: banner.recorderError
            text: qsTr("Dismiss")
            onClicked: Recorder.clearError()
        }
    }
}
