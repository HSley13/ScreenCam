import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// The sidebar: every control edits RecordingSettings; the capture code follows the settings.
Rectangle {
    id: panel

    property bool locked: false  // while recording
    default property alias extraSections: extras.data

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.stroke

    ScrollView {
        id: scroll

        anchors.fill: parent
        anchors.margins: 18
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 24
            enabled: !panel.locked
            opacity: enabled ? 1.0 : 0.6

            SidebarSection {
                title: qsTr("Sources")

                Field {
                    label: qsTr("Record")

                    SegmentedControl {
                        Layout.fillWidth: true
                        options: [
                            { value: RecordingSettings.Screen, label: qsTr("Entire screen") },
                            { value: RecordingSettings.Window, label: qsTr("Window") },
                            { value: RecordingSettings.Region, label: qsTr("Region") }
                        ]
                        value: RecordingSettings.captureTarget
                        onActivated: (value) => {
                            RecordingSettings.captureTarget = value;
                            if (value === RecordingSettings.Window) {
                                Capture.refreshWindows();
                            }
                        }
                    }
                }

                Field {
                    label: qsTr("Screen")
                    visible: RecordingSettings.captureTarget !== RecordingSettings.Window

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StyledComboBox {
                            Layout.fillWidth: true
                            model: Devices.screens
                            selectedValue: RecordingSettings.screenId
                            onPicked: (value) => RecordingSettings.screenId = value
                        }

                        StyledButton {
                            text: qsTr("Show")
                            onClicked: Capture.identifyTarget()
                        }
                    }
                }

                Field {
                    label: qsTr("Window")
                    hint: qsTr("%n app window(s)", "", Capture.windows.length)
                    visible: RecordingSettings.captureTarget === RecordingSettings.Window

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StyledComboBox {
                            Layout.fillWidth: true
                            model: Capture.windows
                            selectedValue: Capture.selectedWindow
                            displayText: Capture.selectedWindow < 0 ? qsTr("Choose a window…") : currentText
                            onPicked: (value) => Capture.selectedWindow = value

                            onAboutToOpen: Capture.refreshWindows()
                        }

                        StyledButton {
                            text: qsTr("Show")
                            enabled: Capture.selectedWindow >= 0
                            onClicked: Capture.identifyTarget()
                        }

                        StyledButton {
                            text: qsTr("Refresh")
                            onClicked: Capture.refreshWindows()
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textMuted
                        font.pixelSize: 11
                        text: Qt.platform.os === "osx"
                              ? qsTr("Only app windows are listed — not the menu bar, its icons or the Dock. "
                                     + "Minimized windows and windows on other desktops appear once they're on screen.")
                              : qsTr("Only app windows are listed — not the taskbar, panels or tray icons.")
                    }
                }

                Field {
                    label: qsTr("Area")
                    hint: RecordingSettings.region.width > 0
                          ? Capture.screenSize.width + " × " + Capture.screenSize.height + " px" : qsTr("none yet")
                    visible: RecordingSettings.captureTarget === RecordingSettings.Region

                    StyledButton {
                        Layout.fillWidth: true
                        accent: RecordingSettings.region.width === 0
                        text: RecordingSettings.region.width > 0 ? qsTr("Select a different area…") : qsTr("Select area…")
                        onClicked: panel.selectRegion()
                    }
                }

                Field {
                    label: qsTr("Camera")

                    StyledComboBox {
                        Layout.fillWidth: true
                        model: Devices.cameras
                        selectedValue: RecordingSettings.cameraId
                        onPicked: (value) => RecordingSettings.cameraId = value
                    }

                    Label {
                        visible: !Devices.cameraAllowed
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Camera access is off. Allow ScreenCam in your system's privacy settings.")
                        color: Theme.warning
                        font.pixelSize: 11
                    }
                }

                Field {
                    label: qsTr("Microphone")

                    StyledComboBox {
                        Layout.fillWidth: true
                        model: Devices.microphones
                        selectedValue: RecordingSettings.microphoneId
                        onPicked: (value) => RecordingSettings.microphoneId = value
                    }

                    LevelMeter {
                        Layout.fillWidth: true
                        visible: RecordingSettings.microphoneId.length > 0
                        level: meter.level
                    }

                    Label {
                        visible: !Devices.microphoneAllowed
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Microphone access is off. Allow ScreenCam in your system's privacy settings.")
                        color: Theme.warning
                        font.pixelSize: 11
                    }

                    AudioLevelMeter {
                        id: meter
                        deviceId: RecordingSettings.microphoneId
                        // No hidden mic use in the background.
                        active: Devices.microphoneAllowed && panel.Window.active
                    }
                }
            }

            ColumnLayout {
                id: extras
                Layout.fillWidth: true
                spacing: 24
            }

            SidebarSection {
                title: qsTr("Output")

                Field {
                    label: qsTr("Resolution")
                    hint: qsTr("keeps your screen's shape")

                    StyledComboBox {
                        Layout.fillWidth: true
                        model: RecordingSettings.resolutionOptions()
                        textRole: "label"
                        valueRole: "value"
                        selectedValue: RecordingSettings.resolution
                        onPicked: (value) => RecordingSettings.resolution = value
                    }
                }

                Field {
                    label: qsTr("Frame rate")

                    SegmentedControl {
                        Layout.fillWidth: true
                        options: [
                            { value: 24, label: qsTr("24 fps") },
                            { value: 30, label: qsTr("30 fps") },
                            { value: 60, label: qsTr("60 fps") }
                        ]
                        value: RecordingSettings.frameRate
                        onActivated: (value) => RecordingSettings.frameRate = value
                    }
                }

                Field {
                    label: qsTr("Countdown")

                    SegmentedControl {
                        Layout.fillWidth: true
                        options: [
                            { value: 0, label: qsTr("Off") },
                            { value: 3, label: qsTr("3 s") },
                            { value: 5, label: qsTr("5 s") },
                            { value: 10, label: qsTr("10 s") }
                        ]
                        value: RecordingSettings.countdownSeconds
                        onActivated: (value) => RecordingSettings.countdownSeconds = value
                    }
                }

                StyledSwitch {
                    Layout.fillWidth: true
                    text: qsTr("Show mouse pointer")
                    checked: RecordingSettings.showPointer
                    onToggled: RecordingSettings.showPointer = checked
                }

                StyledSwitch {
                    Layout.fillWidth: true
                    visible: RecordingSettings.showPointer
                    text: qsTr("Highlight pointer")
                    checked: RecordingSettings.highlightPointer
                    onToggled: RecordingSettings.highlightPointer = checked
                }

                StyledSwitch {
                    Layout.fillWidth: true
                    text: qsTr("Hide ScreenCam while recording")
                    checked: RecordingSettings.hideWindowWhileRecording
                    onToggled: RecordingSettings.hideWindowWhileRecording = checked
                }

                Field {
                    label: qsTr("Save to")

                    Label {
                        Layout.fillWidth: true
                        text: RecordingSettings.outputFolder
                        color: Theme.textMuted
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StyledButton {
                            Layout.fillWidth: true
                            text: qsTr("Change…")
                            onClicked: folderDialog.open()
                        }

                        StyledButton {
                            Layout.fillWidth: true
                            text: qsTr("Open folder")
                            onClicked: RecordingSettings.openOutputFolder()
                        }
                    }
                }
            }
        }
    }

    function selectRegion() {
        const screens = Devices.screens;
        let entry = screens.length > 0 ? screens[0] : null;
        for (let i = 0; i < screens.length; i++) {
            if (screens[i].id === RecordingSettings.screenId) {
                entry = screens[i];
            }
        }
        if (entry === null) {
            return;
        }
        regionSelectorComponent.createObject(null, {
            screenId: entry.id,
            screenGeometry: entry.geometry,
            pixelRatio: entry.pixelRatio
        }).show();
    }

    Component {
        id: regionSelectorComponent
        RegionSelector {
            onClosing: destroy()
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose where recordings are saved")
        currentFolder: RecordingSettings.outputFolderUrl
        onAccepted: RecordingSettings.outputFolderUrl = selectedFolder
    }
}
