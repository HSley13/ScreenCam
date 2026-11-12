import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// The right-hand sidebar. Every control reads from and writes to RecordingSettings, which
// persists the choice; the capture code reacts to those settings, not to this UI.
Rectangle {
    id: panel

    property bool locked: false  // true while recording: settings can't change mid-take
    default property alias extraSections: extras.data

    readonly property bool screenMode: RecordingSettings.mode !== RecordingSettings.CameraOnly
    readonly property bool cameraMode: RecordingSettings.mode !== RecordingSettings.ScreenOnly

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
                title: qsTr("Record")

                SegmentedControl {
                    Layout.fillWidth: true
                    options: [
                        { value: RecordingSettings.ScreenAndCamera, label: qsTr("Screen + Cam") },
                        { value: RecordingSettings.ScreenOnly, label: qsTr("Screen") },
                        { value: RecordingSettings.CameraOnly, label: qsTr("Camera") }
                    ]
                    value: RecordingSettings.mode
                    onActivated: (value) => RecordingSettings.mode = value
                }
            }

            SidebarSection {
                title: qsTr("Sources")

                Field {
                    label: qsTr("Record")
                    visible: panel.screenMode

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
                    visible: panel.screenMode && RecordingSettings.captureTarget !== RecordingSettings.Window

                    StyledComboBox {
                        Layout.fillWidth: true
                        model: Devices.screens
                        selectedValue: RecordingSettings.screenId
                        onPicked: (value) => RecordingSettings.screenId = value
                    }
                }

                Field {
                    label: qsTr("Window")
                    hint: qsTr("%n app window(s)", "", Capture.windows.length)
                    visible: panel.screenMode && RecordingSettings.captureTarget === RecordingSettings.Window

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StyledComboBox {
                            Layout.fillWidth: true
                            model: Capture.windows
                            selectedValue: Capture.selectedWindow
                            displayText: Capture.selectedWindow < 0 ? qsTr("Choose a window…") : currentText
                            onPicked: (value) => Capture.selectedWindow = value
                            // Apps open and close all the time: re-read the list each time it opens.
                            onAboutToOpen: Capture.refreshWindows()
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
                    visible: panel.screenMode && RecordingSettings.captureTarget === RecordingSettings.Region

                    StyledButton {
                        Layout.fillWidth: true
                        accent: RecordingSettings.region.width === 0
                        text: RecordingSettings.region.width > 0 ? qsTr("Select a different area…") : qsTr("Select area…")
                        onClicked: panel.selectRegion()
                    }
                }

                Field {
                    label: qsTr("Camera")
                    visible: panel.cameraMode

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
                        // Only listen while the window is in front: no hidden mic use in the background.
                        active: Devices.microphoneAllowed && panel.Window.active
                    }
                }
            }

            // Sections added by other features (camera bubble, ...) are inserted here.
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
                    visible: panel.screenMode

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

    // Opens the region selector on the chosen screen.
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
