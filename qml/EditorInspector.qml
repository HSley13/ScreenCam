pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The editor's right-hand panel: everything about the segment under the playhead.
// The bubble controls are the same BubbleSection the recorder uses, pointed at the
// segment's style instead of the app-wide default.
Rectangle {
    id: inspector

    required property ProjectEditor editor
    property size canvasSize: Qt.size(1, 1)  // the preview canvas, for corner placement
    property real positionMs: 0

    signal splitRequested()

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

            SidebarSection {
                title: qsTr("Segment")

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Part %1 of %2  ·  %3 – %4")
                          .arg(inspector.editor.currentIndex + 1)
                          .arg(inspector.editor.segmentCount)
                          .arg(inspector.editor.formatTime(inspector.editor.currentStartMs))
                          .arg(inspector.editor.formatTime(inspector.editor.currentEndMs))
                    color: Theme.text
                    font.pixelSize: 13
                    font.features: { "tnum": 1 }
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textMuted
                    font.pixelSize: 12
                    text: qsTr("Drag yourself in the preview to move the camera for this part. "
                               + "Split where the background changes to place it differently from there on.")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StyledButton {
                        Layout.fillWidth: true
                        text: qsTr("Split here (S)")
                        // segmentCount: re-check after every split/delete (QML can't see inside the call).
                        enabled: inspector.editor.segmentCount > 0
                                 && inspector.editor.canSplitAt(Math.round(inspector.positionMs))
                        onClicked: inspector.splitRequested()
                    }

                    StyledButton {
                        Layout.fillWidth: true
                        text: qsTr("Delete part")
                        enabled: inspector.editor.segmentCount > 1
                        onClicked: inspector.editor.removeCurrent()
                    }
                }

                StyledSwitch {
                    Layout.fillWidth: true
                    visible: inspector.editor.hasCamera
                    text: qsTr("Show camera in this part")
                    checked: inspector.editor.cameraVisible
                    onToggled: inspector.editor.cameraVisible = checked
                }

                Field {
                    label: qsTr("Quick position")
                    visible: inspector.editor.hasCamera
                    enabled: inspector.editor.cameraVisible
                    opacity: enabled ? 1.0 : 0.4

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
                                onClicked: inspector.editor.style.placeInCorner(modelData.corner,
                                                                                inspector.canvasSize.width,
                                                                                inspector.canvasSize.height)
                            }
                        }
                    }
                }

                StyledButton {
                    Layout.fillWidth: true
                    visible: inspector.editor.segmentCount > 1
                    text: qsTr("Use this look for all parts")
                    onClicked: inspector.editor.applyToAll()
                }
            }

            BubbleSection {
                visible: inspector.editor.hasCamera
                enabled: inspector.editor.cameraVisible
                opacity: enabled ? 1.0 : 0.4
                editorMode: true
                bubbleStyle: inspector.editor.style
            }

            SidebarSection {
                title: qsTr("Project")
                visible: inspector.editor.hasCamera

                Field {
                    label: qsTr("Camera sync")
                    hint: {
                        const delta = inspector.editor.cameraOffsetMs - inspector.editor.recordedCameraOffsetMs;
                        return delta === 0 ? qsTr("as recorded") : (delta > 0 ? "+" : "") + delta + " ms";
                    }

                    // If your lips and voice don't line up, nudge the camera later (+) or earlier (−).
                    StyledSlider {
                        Layout.fillWidth: true
                        from: inspector.editor.recordedCameraOffsetMs - 1000
                        to: inspector.editor.recordedCameraOffsetMs + 1000
                        stepSize: 10
                        value: inspector.editor.cameraOffsetMs
                        onMoved: inspector.editor.cameraOffsetMs = Math.round(value)
                    }
                }
            }
        }
    }
}
