import QtQuick
import QtQuick.Controls

// One window per open project; closing it leaves the recorder open.
ApplicationWindow {
    id: editorWindow

    required property string folder
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
    // Not a dialog of the recorder: it can go behind it and minimize on its own.
    transientParent: null

    onClosing: editorWindow.finished()
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
