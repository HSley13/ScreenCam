import QtQuick

// Horizontal input-level bar: green -> amber -> red.
Item {
    id: root

    property real level: 0.0  // 0..1

    implicitHeight: 6

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: "#232838"
    }

    Item {
        height: parent.height
        width: Math.max(0, Math.min(1, root.level)) * root.width
        clip: true
        Behavior on width { NumberAnimation { duration: 70 } }

        // Full-width gradient clipped by the parent, so colours stay put as the bar grows.
        Rectangle {
            width: root.width
            height: root.height
            radius: height / 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.success }
                GradientStop { position: 0.72; color: Theme.warning }
                GradientStop { position: 1.0; color: Theme.record }
            }
        }
    }
}
