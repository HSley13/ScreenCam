pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// A ComboBox bound to a setting: shows `selectedValue`, emits picked(value). Choosing an item
// overwrites the currentIndex binding; onActivated restores it so changes from C++ still show.
ComboBox {
    id: control

    property var selectedValue
    signal picked(var value)
    // For lists that change on their own (open windows).
    signal aboutToOpen()

    textRole: "name"
    valueRole: "id"
    implicitHeight: 38
    font.pixelSize: 13
    currentIndex: indexFor(model, selectedValue)

    function indexFor(list, value) {
        if (!list || list.length === undefined) {
            return -1;
        }
        for (let i = 0; i < list.length; i++) {
            if (list[i][valueRole] === value) {
                return i;
            }
        }
        return list.length > 0 ? 0 : -1;
    }

    onActivated: (index) => {
        picked(model[index][valueRole]);
        currentIndex = Qt.binding(() => indexFor(model, selectedValue));
    }

    contentItem: Text {
        leftPadding: 12
        rightPadding: control.indicator.width + 8
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.text : Theme.textMuted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 12
        y: (control.height - height) / 2
        text: "▾"
        color: Theme.textMuted
        font.pixelSize: 12
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.hovered ? Theme.surfaceRaised : "#181c27"
        border.color: control.visualFocus || control.popup.visible ? Theme.accent : Theme.stroke
        Behavior on color { ColorAnimation { duration: Theme.animationFast } }
    }

    delegate: ItemDelegate {
        id: item

        required property var modelData
        required property int index

        width: ListView.view ? ListView.view.width : control.width
        height: 34
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: item.modelData[control.textRole]
            color: item.index === control.currentIndex ? Theme.accent : Theme.text
            font: control.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 7
            color: item.highlighted ? Theme.accentSoft : "transparent"
        }
    }

    popup: Popup {
        onAboutToShow: control.aboutToOpen()
        y: control.height + 4
        width: control.width
        padding: 6
        implicitHeight: Math.min(contentItem.implicitHeight + 12, 320)

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.surfaceRaised
            border.color: Theme.stroke
        }
    }
}
