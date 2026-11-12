pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// Sidebar section "CAMERA": bubble look in Screen + Cam mode, framing in Camera-only mode.
SidebarSection {
    id: section

    // Canvas size of the preview, needed to place the bubble in a corner.
    property size canvasSize: Qt.size(1920, 1080)

    readonly property bool bubbleMode: RecordingSettings.mode === RecordingSettings.ScreenAndCamera
    readonly property bool cameraOnly: RecordingSettings.mode === RecordingSettings.CameraOnly

    title: qsTr("Camera")
    visible: (bubbleMode || cameraOnly) && RecordingSettings.cameraId.length > 0

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
            shape: BubbleStyle.shape
            onPicked: (shape) => BubbleStyle.shape = shape
        }
    }

    Field {
        label: qsTr("Size")
        hint: Math.round(BubbleStyle.size * 100) + "%"
        visible: section.bubbleMode

        StyledSlider {
            Layout.fillWidth: true
            from: 0.08
            to: 0.6
            value: BubbleStyle.size
            onMoved: BubbleStyle.size = value
        }
    }

    Field {
        label: qsTr("Corner roundness")
        hint: Math.round(BubbleStyle.roundness * 100) + "%"
        visible: section.bubbleMode
        enabled: BubbleStyle.shape !== BubbleStyle.Circle
        opacity: enabled ? 1.0 : 0.4

        StyledSlider {
            Layout.fillWidth: true
            from: 0.0
            to: 1.0
            value: BubbleStyle.roundness
            onMoved: BubbleStyle.roundness = value
        }
    }

    Field {
        label: qsTr("Border")
        hint: BubbleStyle.borderWidth < 0.5 ? qsTr("off") : Math.round(BubbleStyle.borderWidth) + " px"
        visible: section.bubbleMode

        StyledSlider {
            Layout.fillWidth: true
            from: 0
            to: 16
            stepSize: 1
            value: BubbleStyle.borderWidth
            onMoved: BubbleStyle.borderWidth = value
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: ["#ffffff", "#7c5cff", "#3ddc97", "#ffb547", "#ff4d5e", "#111318"]

                delegate: Rectangle {
                    id: swatch

                    required property string modelData
                    readonly property bool selected: Qt.colorEqual(BubbleStyle.borderColor, modelData)

                    width: 24
                    height: 24
                    radius: 12
                    color: modelData
                    border.width: selected ? 3 : 1
                    border.color: selected ? Theme.accent : Theme.stroke

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: BubbleStyle.borderColor = swatch.modelData
                    }
                }
            }
        }
    }

    StyledSwitch {
        Layout.fillWidth: true
        visible: section.bubbleMode
        text: qsTr("Drop shadow")
        checked: BubbleStyle.shadow
        onToggled: BubbleStyle.shadow = checked
    }

    StyledSwitch {
        Layout.fillWidth: true
        text: qsTr("Mirror camera")
        checked: BubbleStyle.mirrored
        onToggled: BubbleStyle.mirrored = checked
    }

    Field {
        label: qsTr("Position")
        hint: qsTr("or drag it on the preview")
        visible: section.bubbleMode

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 8
            columnSpacing: 8

            Repeater {
                model: [
                    { corner: BubbleStyle.TopLeft, label: qsTr("↖ Top left") },
                    { corner: BubbleStyle.TopRight, label: qsTr("Top right ↗") },
                    { corner: BubbleStyle.BottomLeft, label: qsTr("↙ Bottom left") },
                    { corner: BubbleStyle.BottomRight, label: qsTr("Bottom right ↘") }
                ]

                delegate: StyledButton {
                    required property var modelData

                    Layout.fillWidth: true
                    text: modelData.label
                    onClicked: BubbleStyle.placeInCorner(modelData.corner, section.canvasSize.width,
                                                         section.canvasSize.height)
                }
            }
        }
    }
}
