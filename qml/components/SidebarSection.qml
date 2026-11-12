import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A titled group in the sidebar ("SOURCES", "CAMERA", ...). Children stack vertically.
ColumnLayout {
    id: root

    property string title
    default property alias content: body.data

    Layout.fillWidth: true
    spacing: 10

    Label {
        text: root.title.toUpperCase()
        color: Theme.textMuted
        font.pixelSize: 11
        font.letterSpacing: 1.2
        font.weight: Font.DemiBold
    }

    ColumnLayout {
        id: body
        Layout.fillWidth: true
        spacing: 12
    }
}
