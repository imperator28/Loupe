import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: control

    property QtObject theme
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"
    readonly property color mutedForeground: theme ? theme.muted : "#53636c"
    palette.text: foreground
    palette.buttonText: foreground
    palette.windowText: foreground
    palette.highlightedText: foreground

    implicitHeight: 32
    leftPadding: 10
    rightPadding: 30

    contentItem: Text {
        text: control.displayText
        color: control.enabled ? control.foreground : control.mutedForeground
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 10
        y: (control.height - height) / 2
        text: "v"
        color: control.enabled ? control.foreground : control.mutedForeground
        font.bold: true
    }

    background: Rectangle {
        radius: 4
        color: control.theme ? control.theme.control : "#dde5e9"
        border.color: control.activeFocus && control.theme ? control.theme.accent
                                                          : control.theme ? control.theme.border : "#b9c5cb"
        opacity: control.enabled ? 1 : 0.58
    }

    delegate: ItemDelegate {
        required property var modelData
        required property int index
        width: control.width
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.textRole.length > 0 && modelData && modelData[control.textRole] !== undefined
                  ? modelData[control.textRole] : modelData
            color: control.foreground
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: parent.highlighted ? (control.theme ? control.theme.selection : "#a8ddd7")
                                      : control.theme ? control.theme.surfaceRaised : "#ffffff"
        }
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 2, 280)
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            radius: 4
            color: control.theme ? control.theme.surfaceRaised : "#ffffff"
            border.color: control.theme ? control.theme.border : "#b9c5cb"
        }
    }
}
