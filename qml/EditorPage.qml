pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

// Plays a project and lets you move the camera bubble over time. screenPlayer (with the
// audio) is the master clock; syncCamera() keeps cameraPlayer at t - cameraOffsetMs.
Item {
    id: page

    required property string folder
    readonly property string projectName: project.name
    signal closeRequested()

    readonly property bool playing: screenPlayer.playbackState === MediaPlayer.PlayingState
    // While scrubbing, the requested time: the player's position lags behind the mouse.
    property real playheadMs: 0

    property string exportedName: ""

    ProjectEditor {
        id: project
        onExportFinished: (path) => page.exportedName = path.substring(path.lastIndexOf("/") + 1)
    }

    Component.onCompleted: project.open(page.folder)
    Component.onDestruction: project.close()

    function seek(ms: real) {
        const target = Math.max(0, Math.min(ms, project.durationMs));
        page.playheadMs = target;
        screenPlayer.position = target;
        project.selectAt(Math.round(target));
        page.syncCamera(target, true);
    }

    function togglePlay() {
        if (page.playing) {
            screenPlayer.pause();
        } else {
            if (screenPlayer.position >= project.durationMs - 50) {
                page.seek(0);  // at the end: from the start
            }
            screenPlayer.play();
        }
    }

    function splitAtPlayhead() {
        project.splitAt(Math.round(page.playheadMs));
    }

    // Without `force` only real drift is corrected: every seek makes the decoder stutter.
    function syncCamera(timeMs: real, force: bool) {
        if (!project.hasCamera) {
            return;
        }
        const target = timeMs - project.cameraOffsetMs;
        if (target < 0 || (cameraPlayer.duration > 0 && target > cameraPlayer.duration)) {
            if (cameraPlayer.playbackState === MediaPlayer.PlayingState) {
                cameraPlayer.pause();
            }
            if (force) {
                cameraPlayer.position = Math.max(0, Math.min(target, cameraPlayer.duration));
            }
            return;
        }
        if (force || Math.abs(cameraPlayer.position - target) > 120) {
            cameraPlayer.position = target;
        }
        if (page.playing && cameraPlayer.playbackState !== MediaPlayer.PlayingState) {
            cameraPlayer.play();
        } else if (!page.playing && cameraPlayer.playbackState === MediaPlayer.PlayingState) {
            cameraPlayer.pause();
        }
    }

    MediaPlayer {
        id: screenPlayer
        source: project.screenUrl
        videoOutput: screenView
        audioOutput: AudioOutput {}

        property bool prepared: false

        // Pausing a freshly loaded player shows its first frame. Only once: seeking re-enters
        // LoadedMedia, and pausing again would loop.
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia && !prepared) {
                prepared = true;
                project.durationMs = duration;
                pause();
            } else if (mediaStatus === MediaPlayer.EndOfMedia) {
                page.playheadMs = project.durationMs;
            }
        }
        onPositionChanged: {
            if (!timeline.scrubbing) {
                page.playheadMs = position;
                project.selectAt(position);
            }
        }
        onPlaybackStateChanged: {
            if (prepared) {
                page.syncCamera(position, true);
            }
        }
    }

    MediaPlayer {
        id: cameraPlayer
        source: project.cameraUrl
        videoOutput: bubble.video

        property bool prepared: false

        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia && !prepared) {
                prepared = true;
                pause();
                page.syncCamera(screenPlayer.position, true);
            }
        }
    }

    // Two decoders never stay perfectly in step.
    Timer {
        interval: 500
        repeat: true
        running: page.playing
        onTriggered: page.syncCamera(screenPlayer.position, false)
    }

    Connections {
        target: project
        // Nudging "Camera sync" should show its effect right away.
        function onCameraOffsetChanged() {
            page.syncCamera(screenPlayer.position, true);
        }
    }

    Shortcut {
        sequences: ["Space"]
        onActivated: page.togglePlay()
    }

    Shortcut {
        sequences: ["S"]
        onActivated: page.splitAtPlayhead()
    }

    Shortcut {
        sequences: [StandardKey.Delete, "Backspace"]
        onActivated: project.removeCurrent()
    }

    Shortcut {
        sequences: ["Left"]
        onActivated: page.seek(page.playheadMs - 1000)
    }

    Shortcut {
        sequences: ["Right"]
        onActivated: page.seek(page.playheadMs + 1000)
    }

    Shortcut {
        sequences: ["Home"]
        onActivated: page.seek(0)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            StyledButton {
                text: qsTr("Close")
                onClicked: page.closeRequested()
            }

            Label {
                text: project.loaded ? project.name : qsTr("Editor")
                color: Theme.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
                Layout.maximumWidth: 420
            }

            Label {
                text: {
                    if (project.errorText.length > 0) {
                        return project.errorText;
                    }
                    if (project.exporting) {
                        return qsTr("Exporting…  you can keep editing; changes go into the next export");
                    }
                    if (page.exportedName.length > 0) {
                        return qsTr("Exported “%1”").arg(page.exportedName);
                    }
                    return project.dirty ? qsTr("Saving…") : qsTr("All changes saved");
                }
                color: project.errorText.length > 0 ? Theme.record : Theme.textMuted
                font.pixelSize: 12
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            StyledButton {
                text: qsTr("Show in folder")
                enabled: project.loaded
                onClicked: project.showInFileManager()
            }

            StyledButton {
                visible: project.lastExport.length > 0 && !project.exporting
                text: qsTr("Play video")
                onClicked: project.playLastExport()
            }

            // In place of the export button.
            RowLayout {
                visible: project.exporting
                spacing: 10

                Rectangle {
                    implicitWidth: 160
                    implicitHeight: 6
                    radius: 3
                    color: "#2a2f3f"

                    Rectangle {
                        width: parent.width * project.exportProgress
                        height: parent.height
                        radius: 3
                        color: Theme.accent
                        Behavior on width { NumberAnimation { duration: Theme.animationFast } }
                    }
                }

                Label {
                    text: Math.round(project.exportProgress * 100) + "%"
                    color: Theme.text
                    font.pixelSize: 12
                    font.features: { "tnum": 1 }
                    Layout.preferredWidth: 34
                }

                StyledButton {
                    text: qsTr("Cancel")
                    onClicked: project.cancelExport()
                }
            }

            StyledButton {
                visible: !project.exporting
                enabled: project.loaded
                accent: true
                text: project.lastExport.length > 0 ? qsTr("Export again") : qsTr("Export video")
                onClicked: {
                    page.exportedName = "";
                    project.exportVideo();
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                Rectangle {
                    id: stage
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.surface
                    radius: Theme.radius
                    border.color: Theme.stroke

                    Item {
                        id: canvas

                        readonly property real sourceWidth: Math.max(1, project.canvasSize.width)
                        readonly property real sourceHeight: Math.max(1, project.canvasSize.height)
                        readonly property real fit: Math.min((stage.width - 32) / sourceWidth,
                                                             (stage.height - 32) / sourceHeight)

                        anchors.centerIn: parent
                        width: Math.max(1, sourceWidth * fit)
                        height: Math.max(1, sourceHeight * fit)
                        clip: true

                        Rectangle {
                            anchors.fill: parent
                            color: "black"
                        }

                        VideoOutput {
                            id: screenView
                            anchors.fill: parent
                            fillMode: VideoOutput.PreserveAspectFit
                        }

                        // Same component and C++ geometry as the export.
                        CameraBubble {
                            id: bubble
                            canvasSize: Qt.size(canvas.width, canvas.height)
                            bubbleStyle: project.style
                            animated: false
                            live: project.hasCamera
                            visible: project.hasCamera && project.cameraVisible
                        }

                        // Play/pause (the bubble's own handlers win).
                        TapHandler {
                            onTapped: page.togglePlay()
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: !project.loaded
                        text: project.errorText
                        color: Theme.textMuted
                        wrapMode: Text.WordWrap
                        width: parent.width * 0.7
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: transport.implicitHeight + 28
                    color: Theme.surface
                    radius: Theme.radius
                    border.color: Theme.stroke

                    ColumnLayout {
                        id: transport
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Rectangle {
                                implicitWidth: 40
                                implicitHeight: 40
                                radius: 20
                                color: playArea.pressed ? Qt.darker(Theme.accent, 1.2)
                                                        : playArea.containsMouse ? Qt.lighter(Theme.accent, 1.1) : Theme.accent

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 4
                                    visible: page.playing

                                    Rectangle { width: 4; height: 14; radius: 1; color: "white" }
                                    Rectangle { width: 4; height: 14; radius: 1; color: "white" }
                                }

                                Canvas {
                                    anchors.centerIn: parent
                                    anchors.horizontalCenterOffset: 2
                                    width: 14
                                    height: 16
                                    visible: !page.playing
                                    onPaint: {
                                        const ctx = getContext("2d");
                                        ctx.reset();
                                        ctx.fillStyle = "white";
                                        ctx.beginPath();
                                        ctx.moveTo(0, 0);
                                        ctx.lineTo(width, height / 2);
                                        ctx.lineTo(0, height);
                                        ctx.closePath();
                                        ctx.fill();
                                    }
                                }

                                MouseArea {
                                    id: playArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: page.togglePlay()
                                }
                            }

                            Label {
                                text: project.formatTime(page.playheadMs) + "  /  " + project.formatTime(project.durationMs)
                                color: Theme.text
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                font.features: { "tnum": 1 }
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Label {
                                text: qsTr("Space play  ·  S split  ·  ⌫ delete  ·  ← → seek")
                                color: Theme.textMuted
                                font.pixelSize: 11
                                elide: Text.ElideLeft
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        EditorTimeline {
                            id: timeline
                            Layout.fillWidth: true
                            editor: project
                            positionMs: page.playheadMs
                            onSeekRequested: (ms) => page.seek(ms)
                        }
                    }
                }
            }

            EditorInspector {
                Layout.preferredWidth: Theme.sidebarWidth
                Layout.fillHeight: true
                editor: project
                canvasSize: Qt.size(canvas.width, canvas.height)
                positionMs: page.playheadMs
                onSplitRequested: page.splitAtPlayhead()
            }
        }
    }
}
