import QtQuick
import QtQuick.Controls
import QtQuick.Window

// A small name tag on the screen, window or region being recorded — "Safari — GitHub",
// "Entire screen · Built-in Retina Display" — so with several monitors and many windows you
// always know what's in the video.
//  - While recording: shown the whole time, above the recorded window/region (or inside its
//    top edge when there's no room), at the top centre of a recorded screen.
//  - Before recording: appears for a moment whenever you choose another screen, window or
//    region, or press "Show".
// Clicks go straight through it, and it's excluded from the recording. A recorded window can
// move: its place is re-read a few times per second.
Window {
    id: tag

    readonly property int gap: 6
    readonly property bool wholeScreen: RecordingSettings.captureTarget === RecordingSettings.Screen
    property rect target: Qt.rect(0, 0, 0, 0)
    readonly property bool hasTarget: target.width > 0 && target.height > 0
    property bool identifying: false
    readonly property bool recording: Recorder.busy && Recorder.state !== Recorder.Finalizing
    // Above the window/region, unless that would leave the top of the screen.
    readonly property bool inside: wholeScreen || target.y - height - gap < Screen.virtualY + 4
    // Linux can't keep windows out of a screen recording: a tag drawn on the recorded screen
    // would end up in the video, so there it's only shown before recording.
    readonly property bool wouldBeRecorded: wholeScreen && !WindowTools.canExcludeFromCapture

    title: qsTr("ScreenCam Target")
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.NoDropShadowWindowHint
           | Qt.WindowTransparentForInput | Qt.WindowDoesNotAcceptFocus
           | (Qt.platform.os === "osx" ? 0 : Qt.Tool)
    visible: hasTarget && RecordingSettings.mode !== RecordingSettings.CameraOnly
             && ((recording && !wouldBeRecorded) || (identifying && !Recorder.busy))

    // Sized from the text itself (a label's own size follows the window it fills).
    width: Math.ceil(Math.min(metrics.advanceWidth + 28, Math.max(160, target.width)))
    height: 26
    x: wholeScreen ? target.x + (target.width - width) / 2 : target.x + (inside ? gap : 0)
    y: inside ? target.y + gap : target.y - height - gap

    onVisibleChanged: {
        if (visible) {
            WindowTools.excludeFromCapture(tag);
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
        onTriggered: tag.identifying = false
    }

    Timer {
        interval: 250
        repeat: true
        triggeredOnStart: true
        running: Recorder.busy || tag.identifying
        onTriggered: tag.refresh()
    }

    // Choosing another screen / target / region shows the tag on it.
    Connections {
        target: RecordingSettings
        function onScreenIdChanged() {
            tag.identify();
        }
        function onCaptureTargetChanged() {
            tag.identify();
        }
        function onRegionChanged() {
            tag.identify();
        }
    }

    // ... and so does picking another window (the list also refreshes on its own, which
    // doesn't count: only a different selection does).
    property int shownWindow: -1
    Connections {
        target: Capture
        function onWindowsChanged() {
            if (Capture.selectedWindow !== tag.shownWindow) {
                tag.shownWindow = Capture.selectedWindow;
                if (Capture.selectedWindow >= 0) {
                    tag.identify();
                }
            }
        }
        function onTargetChanged() {
            tag.refresh();
        }
        function onIdentifyRequested() {
            tag.identify();
        }
    }

    TextMetrics {
        id: metrics
        font: label.font
        text: label.text
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke

        Label {
            id: label
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: Theme.text
            font.pixelSize: 12
            font.weight: Font.DemiBold
            text: tag.recording ? Capture.targetLabel : qsTr("Will record: %1").arg(Capture.targetLabel)
        }
    }
}
