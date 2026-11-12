import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A small caption above a control. Children should set Layout.fillWidth: true.
ColumnLayout {
    id: root

    property string label
    property string hint  // optional grey text on the right of the caption
    default property alias content: body.data

    Layout.fillWidth: true
    spacing: 6

    RowLayout {
        Layout.fillWidth: true

        Label {
            text: root.label
            color: Theme.text
            font.pixelSize: 12
            font.weight: Font.Medium
            Layout.fillWidth: true
        }

        Label {
            visible: root.hint.length > 0
            text: root.hint
            color: Theme.textMuted
            font.pixelSize: 11
        }
    }

    ColumnLayout {
        id: body
        Layout.fillWidth: true
        spacing: 6
    }
}
