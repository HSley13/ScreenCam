import QtQuick
import QtQuick.Controls

// The editor in a window of its own (one per open project). Closing it closes just that
// project; the recorder window stays open.
ApplicationWindow {
    id: editorWindow

    required property string folder
    // Closed: whoever opened it forgets it and frees it.
    signal finished()

    function bringToFront() {
        if (editorWindow.visibility === Window.Minimized) {
            editorWindow.showNormal();
        }
        editorWindow.raise();
        editorWindow.requestActivate();
    }

    width: 1240
    height: 800
    minimumWidth: 980
    minimumHeight: 620
    visible: true
    color: Theme.window
    title: page.projectName.length > 0 ? qsTr("%1 — ScreenCam Editor").arg(page.projectName)
                                       : qsTr("ScreenCam Editor")
    // A window of its own, not a dialog of the recorder: it may go behind it, minimize on
    // its own, and doesn't close with it.
    transientParent: null

    onClosing: editorWindow.finished()
    // Like every ScreenCam window: never part of a screen recording.
    onVisibilityChanged: (visibility) => {
        if (visibility !== Window.Hidden) {
            WindowTools.excludeFromCapture(editorWindow);
        }
    }

    EditorPage {
        id: page
        anchors.fill: parent
        anchors.margins: 16
        folder: editorWindow.folder
        onCloseRequested: editorWindow.close()
    }
}
