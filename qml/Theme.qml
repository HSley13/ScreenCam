pragma Singleton
import QtQuick

QtObject {
    readonly property color window: "#0d0f14"
    readonly property color surface: "#151821"
    readonly property color surfaceRaised: "#1d2130"
    readonly property color stroke: "#272c3b"
    readonly property color text: "#e9ebf1"
    readonly property color textMuted: "#8b91a4"
    readonly property color accent: "#7c5cff"
    readonly property color accentSoft: "#2b2452"
    readonly property color record: "#ff4d5e"
    readonly property color success: "#3ddc97"
    readonly property color warning: "#ffb547"

    readonly property int radius: 16
    readonly property int radiusSmall: 10
    readonly property int spacing: 12
    readonly property int sidebarWidth: 340

    readonly property int animationFast: 140
    readonly property int animationNormal: 260
}
