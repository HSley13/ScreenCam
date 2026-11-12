pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// Sidebar section "CAMERA": the bubble's look in Screen + Cam mode (the floating bubble and
// the starting layout in the editor), framing in Camera-only mode.
SidebarSection {
    id: section

    // The style being edited: the app-wide default, or the editor's current segment.
    property var bubbleStyle: BubbleStyle
    // In the editor: always show the bubble controls, never the Camera-only ones.
    property bool editorMode: false

    readonly property bool bubbleMode: editorMode || RecordingSettings.mode === RecordingSettings.ScreenAndCamera
    readonly property bool cameraOnly: !editorMode && RecordingSettings.mode === RecordingSettings.CameraOnly

    title: editorMode ? qsTr("Camera look") : qsTr("Camera")
    visible: editorMode || ((bubbleMode || cameraOnly) && RecordingSettings.cameraId.length > 0)

    // --- Camera-only mode -------------------------------------------------------------
    Field {
        label: qsTr("Orientation")
        visible: section.cameraOnly

        SegmentedControl {
            Layout.fillWidth: true
            options: [
                { value: RecordingSettings.Landscape, label: qsTr("Landscape 16:9") },
                { value: RecordingSettings.Portrait, label: qsTr("Portrait 9:16") }
            ]
            value: RecordingSettings.cameraOnlyOrientation
            onActivated: (value) => RecordingSettings.cameraOnlyOrientation = value
        }
    }

    // --- Bubble ----------------------------------------------------------------------
    Field {
        label: qsTr("Shape")
        visible: section.bubbleMode

        ShapePicker {
            Layout.fillWidth: true
            shape: section.bubbleStyle.shape
            onPicked: (shape) => section.bubbleStyle.shape = shape
        }
    }

    Field {
        label: qsTr("Size")
        hint: Math.round(section.bubbleStyle.size * 100) + "%"
        visible: section.bubbleMode

        StyledSlider {
            Layout.fillWidth: true
            from: 0.08
            to: 0.6
            value: section.bubbleStyle.size
            onMoved: section.bubbleStyle.size = value
        }
    }

    Field {
        label: qsTr("Corner roundness")
        hint: Math.round(section.bubbleStyle.roundness * 100) + "%"
        visible: section.bubbleMode
        enabled: section.bubbleStyle.shape !== BubbleStyle.Circle
        opacity: enabled ? 1.0 : 0.4

        StyledSlider {
            Layout.fillWidth: true
            from: 0.0
            to: 1.0
            value: section.bubbleStyle.roundness
            onMoved: section.bubbleStyle.roundness = value
        }
    }

    Field {
        label: qsTr("Border")
        hint: section.bubbleStyle.borderWidth < 0.5 ? qsTr("off") : Math.round(section.bubbleStyle.borderWidth) + " px"
        visible: section.bubbleMode

        StyledSlider {
            Layout.fillWidth: true
            from: 0
            to: 16
            stepSize: 1
            value: section.bubbleStyle.borderWidth
            onMoved: section.bubbleStyle.borderWidth = value
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: ["#ffffff", "#7c5cff", "#3ddc97", "#ffb547", "#ff4d5e", "#111318"]

                delegate: Rectangle {
                    id: swatch

                    required property string modelData
                    readonly property bool selected: Qt.colorEqual(section.bubbleStyle.borderColor, modelData)

                    width: 24
                    height: 24
                    radius: 12
                    color: modelData
                    border.width: selected ? 3 : 1
                    border.color: selected ? Theme.accent : Theme.stroke

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: section.bubbleStyle.borderColor = swatch.modelData
                    }
                }
            }
        }
    }

    StyledSwitch {
        Layout.fillWidth: true
        visible: section.bubbleMode
        text: qsTr("Drop shadow")
        checked: section.bubbleStyle.shadow
        onToggled: section.bubbleStyle.shadow = checked
    }

    StyledSwitch {
        Layout.fillWidth: true
        text: qsTr("Mirror camera")
        checked: section.bubbleStyle.mirrored
        onToggled: section.bubbleStyle.mirrored = checked
    }
}
