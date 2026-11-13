pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// "Recordings": everything you've recorded, newest first. A project (a .screencam folder of
// three files) opens in the editor with one click; a video plays; "Show" finds it on disk.
Popup {
    id: popup

    signal projectChosen(string location)

    width: 540
    height: Math.min(560, implicitHeight)
    padding: 16
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onAboutToShow: Library.refresh()

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius
        border.color: Theme.stroke
    }

    contentItem: ColumnLayout {
        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Recordings")
                color: Theme.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideMiddle
                text: RecordingSettings.outputFolder
                color: Theme.textMuted
                font.pixelSize: 11
            }
        }

        Label {
            Layout.fillWidth: true
            visible: Library.recordings.length === 0
            wrapMode: Text.WordWrap
            color: Theme.textMuted
            font.pixelSize: 13
            text: qsTr("Nothing recorded yet. Your recordings will appear here.")
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 400)
            visible: count > 0
            clip: true
            spacing: 6
            model: Library.recordings
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row

                required property var modelData

                // Double-click: the main action (edit a project, play a video).
                function primaryAction() {
                    if (row.modelData.isProject) {
                        popup.projectChosen(row.modelData.path);
                        popup.close();
                    } else {
                        Library.play(row.modelData.path);
                    }
                }

                width: list.width - 8
                height: 58
                radius: Theme.radiusSmall
                color: hover.hovered ? Theme.surfaceRaised : "#181c27"
                border.color: Theme.stroke

                HoverHandler {
                    id: hover
                }

                TapHandler {
                    onDoubleTapped: row.primaryAction()
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10

                    // Kind: editable project or finished video.
                    Rectangle {
                        implicitWidth: 34
                        implicitHeight: 34
                        radius: 8
                        color: row.modelData.isProject ? Theme.accentSoft : "#23283a"

                        Text {
                            anchors.centerIn: parent
                            text: row.modelData.isProject ? "✎" : "▶"
                            color: row.modelData.isProject ? Theme.accent : Theme.text
                            font.pixelSize: 15
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: row.modelData.name
                            color: Theme.text
                            font.pixelSize: 13
                            font.weight: Font.Medium
                        }

                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            color: Theme.textMuted
                            font.pixelSize: 11
                            text: {
                                const parts = [row.modelData.date];
                                if (row.modelData.duration.length > 0) {
                                    parts.push(row.modelData.duration);
                                }
                                if (row.modelData.isProject) {
                                    parts.push(row.modelData.exportPath.length > 0 ? qsTr("Project · exported")
                                                                                   : qsTr("Project"));
                                } else {
                                    parts.push(qsTr("Video"));
                                }
                                return parts.join("  ·  ");
                            }
                        }
                    }

                    StyledButton {
                        visible: row.modelData.isProject
                        accent: true
                        text: qsTr("Edit")
                        onClicked: row.primaryAction()
                    }

                    // Projects: play the exported video (if any). Videos: play them.
                    StyledButton {
                        visible: !row.modelData.isProject || row.modelData.exportPath.length > 0
                        text: qsTr("Play")
                        onClicked: Library.play(row.modelData.isProject ? row.modelData.exportPath : row.modelData.path)
                    }

                    StyledButton {
                        text: qsTr("Show")
                        onClicked: Library.reveal(row.modelData.path)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            StyledButton {
                text: qsTr("Open other project…")
                onClicked: projectDialog.open()
            }

            Item {
                Layout.fillWidth: true
            }

            StyledButton {
                text: qsTr("Show folder")
                onClicked: RecordingSettings.openOutputFolder()
            }
        }
    }

    // A project is a ".screencam" folder. macOS shows it as a single file (see Info.plist), so
    // pick that; elsewhere it's a normal folder: open it and pick the project.json inside.
    FileDialog {
        id: projectDialog
        title: qsTr("Open a ScreenCam project")
        currentFolder: RecordingSettings.outputFolderUrl
        nameFilters: [qsTr("ScreenCam projects (*.screencam project.json)")]
        onAccepted: {
            popup.projectChosen(selectedFile.toString());
            popup.close();
        }
    }
}
