pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

// The editor: plays a recorded project and lets you move the camera bubble over time.
//
// Two players run side by side, like two tracks in a video editor:
//   screenPlayer — screen.mp4 with the microphone audio: the *master* clock
//   cameraPlayer — camera.mp4 (no audio), drawn inside the bubble
// The camera file started `cameraOffsetMs` later than the screen, so at video time t the
// camera must be at t - cameraOffsetMs. syncCamera() keeps it there.
Item {
    id: page

    required property string folder
    signal closeRequested()

    readonly property bool playing: screenPlayer.playbackState === MediaPlayer.PlayingState
    // Where the playhead is drawn. While scrubbing, the requested time (the player only
    // reports positions once it has decoded the frame, which lags behind the mouse).
    property real playheadMs: 0

    ProjectEditor {
        id: project
    }

    Component.onCompleted: project.open(page.folder)
    Component.onDestruction: project.close()

    // --- Playback ---------------------------------------------------------------------------
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
                page.seek(0);  // at the end: play from the start again
            }
            screenPlayer.play();
        }
    }

    function splitAtPlayhead() {
        project.splitAt(Math.round(page.playheadMs));
    }

    // Puts the camera player where the screen player is (or will be, for `timeMs`).
    //  force: jump even if it's close (after a seek); otherwise only correct real drift,
    //         because every seek makes the decoder stutter.
    function syncCamera(timeMs: real, force: bool) {
        if (!project.hasCamera) {
            return;
        }
        const target = timeMs - project.cameraOffsetMs;
        // Before the camera's first frame or after its last: hold it still.
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

        // A freshly loaded player shows nothing; pausing it once decodes and shows the first
        // frame. Only once: seeking re-enters LoadedMedia, and pausing there again would loop.
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia && !prepared) {
                prepared = true;
                project.durationMs = duration;  // the file knows its real length
                pause();
            } else if (mediaStatus === MediaPlayer.EndOfMedia) {
                page.playheadMs = project.durationMs;
            }
        }
        onPositionChanged: {
            if (!timeline.scrubbing) {
                page.playheadMs = position;
                project.selectAt(position);  // the bubble follows the segment being played
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

    // Drift check while playing: two decoders never stay perfectly in step.
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

    // --- Keyboard -------------------------------------------------------------------------
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

    // --- Layout -------------------------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // Header: back, project name, save state.
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            StyledButton {
                text: qsTr("‹  New recording")
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
                text: project.errorText.length > 0 ? project.errorText
                                                  : (project.dirty ? qsTr("Saving…") : qsTr("All changes saved"))
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
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                // Preview: the output canvas, letterboxed into the available space.
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

                        // Same component (and same C++ geometry) as the recorder's bubble, so
                        // what you place here is exactly what the export draws.
                        CameraBubble {
                            id: bubble
                            canvasSize: Qt.size(canvas.width, canvas.height)
                            bubbleStyle: project.style
                            animated: false
                            live: project.hasCamera
                            visible: project.hasCamera && project.cameraVisible
                        }

                        // Click the picture to play/pause (the bubble's own handlers win over this).
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

                // Transport + timeline.
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

                            // Play/pause: round accent button with a drawn icon.
                            Rectangle {
                                implicitWidth: 40
                                implicitHeight: 40
                                radius: 20
                                color: playArea.pressed ? Qt.darker(Theme.accent, 1.2)
                                                        : playArea.containsMouse ? Qt.lighter(Theme.accent, 1.1) : Theme.accent

                                // Pause bars.
                                Row {
                                    anchors.centerIn: parent
                                    spacing: 4
                                    visible: page.playing

                                    Rectangle { width: 4; height: 14; radius: 1; color: "white" }
                                    Rectangle { width: 4; height: 14; radius: 1; color: "white" }
                                }

                                // Play triangle.
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
