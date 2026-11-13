import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Marks what is being recorded — a screen, a window or a region — with a coloured frame and a
// name tag ("● Recording · Safari — GitHub"), so with several monitors and many windows you
// always know what's in the video.
//  - While recording: shown the whole time (red; amber while paused).
//  - Before recording: flashes for a moment whenever you choose another screen, window or
//    region ("This will be recorded"), so you can see which monitor/window you picked.
// Clicks go straight through it, and it's excluded from the recording. A recorded window can
// move: its place is re-read a few times per second.
Window {
    id: outline

    readonly property int stroke: 3
    readonly property int tagHeight: 26
    // A window or region is framed from the outside (nothing of it is covered). A whole
    // screen can only be framed from the inside, along its edges.
    readonly property bool wholeScreen: RecordingSettings.captureTarget === RecordingSettings.Screen
    readonly property int margin: wholeScreen ? 0 : 4
    property rect target: Qt.rect(0, 0, 0, 0)
    readonly property bool hasTarget: target.width > 0 && target.height > 0
    property bool identifying: false
    readonly property bool recording: Recorder.busy && Recorder.state !== Recorder.Finalizing
    // Tag above the frame, unless the frame is a whole screen or that would leave the screen.
    readonly property bool tagInside: wholeScreen || target.y - tagHeight - margin < Screen.virtualY + 4
    readonly property color frameColor: !recording ? Theme.accent
                                      : Recorder.state === Recorder.Paused ? Theme.warning
                                      : Recorder.state === Recorder.Countdown ? Theme.accent : Theme.record
    // Linux can't keep windows out of a screen recording: a frame drawn over the recorded
    // screen would end up in the video, so there it's only shown before recording.
    readonly property bool frameWouldBeRecorded: wholeScreen && !WindowTools.canExcludeFromCapture

    title: qsTr("ScreenCam Outline")
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.NoDropShadowWindowHint
           | Qt.WindowTransparentForInput | Qt.WindowDoesNotAcceptFocus
           | (Qt.platform.os === "osx" ? 0 : Qt.Tool)
    visible: hasTarget && RecordingSettings.mode !== RecordingSettings.CameraOnly
             && ((recording && !frameWouldBeRecorded) || (identifying && !Recorder.busy))

    x: target.x - margin
    y: target.y - margin - (tagInside ? 0 : tagHeight)
    width: target.width + 2 * margin
    height: target.height + 2 * margin + (tagInside ? 0 : tagHeight)

    onVisibleChanged: {
        if (visible) {
            WindowTools.excludeFromCapture(outline);
        }
    }

    function refresh() {
        const bounds = Capture.targetBounds();
        if (bounds.x !== target.x || bounds.y !== target.y || bounds.width !== target.width
                || bounds.height !== target.height) {
            target = bounds;
        }
    }

    // Show what the current choice is, for a couple of seconds.
    function identify() {
        refresh();
        identifying = true;
        identifyTimer.restart();
    }

    Timer {
        id: identifyTimer
        interval: 2500
        onTriggered: outline.identifying = false
    }

    Timer {
        interval: 250
        repeat: true
        triggeredOnStart: true
        running: Recorder.busy || outline.identifying
        onTriggered: outline.refresh()
    }

    // Choosing another screen / target / region flashes the frame on it.
    Connections {
        target: RecordingSettings
        function onScreenIdChanged() {
            outline.identify();
        }
        function onCaptureTargetChanged() {
            outline.identify();
        }
        function onRegionChanged() {
            outline.identify();
        }
    }

    // ... and so does picking another window (the list also refreshes on its own, which
    // doesn't count: only a different selection does).
    property int shownWindow: -1
    Connections {
        target: Capture
        function onWindowsChanged() {
            if (Capture.selectedWindow !== outline.shownWindow) {
                outline.shownWindow = Capture.selectedWindow;
                if (Capture.selectedWindow >= 0) {
                    outline.identify();
                }
            }
        }
        function onTargetChanged() {
            outline.refresh();
        }
        function onIdentifyRequested() {
            outline.identify();
        }
    }

    Rectangle {
        id: frame
        x: 0
        y: outline.tagInside ? 0 : outline.tagHeight
        width: outline.width
        height: outline.height - y
        color: "transparent"
        radius: outline.wholeScreen ? 0 : 8
        border.width: outline.wholeScreen ? outline.stroke + 1 : outline.stroke
        border.color: outline.frameColor
    }

    // "● Recording · Safari — GitHub": top-left for windows/regions, top-centre for screens.
    Rectangle {
        x: outline.wholeScreen ? (outline.width - width) / 2 : outline.tagInside ? outline.margin + 8 : 0
        y: outline.tagInside ? outline.margin + 8 : 0
        width: Math.min(tagText.implicitWidth + 20, outline.width)
        height: outline.tagHeight - 4
        radius: height / 2
        color: outline.frameColor

        Label {
            id: tagText
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: "white"
            font.pixelSize: 12
            font.weight: Font.DemiBold
            text: {
                const what = Capture.targetLabel;
                if (!outline.recording) {
                    return qsTr("This will be recorded · %1").arg(what);
                }
                switch (Recorder.state) {
                case Recorder.Countdown:
                    return qsTr("Will record · %1").arg(what);
                case Recorder.Paused:
                    return qsTr("Paused · %1").arg(what);
                default:
                    return qsTr("● Recording · %1").arg(what);
                }
            }
        }
    }
}
