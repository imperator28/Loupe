import QtQuick
import QtQuick.Controls
import QtQuick.Effects

// Behind a sheet (What's New, About): the window, blurred and lightly tinted, so the sheet
// reads as raised above it. Put in the window's overlay, below the popup. The overlay
// cannot blur itself, so each of the window's parts to show through (menu bar, header,
// content) is captured and blurred where it is.
Item {
    id: root

    property QtObject theme
    property bool shown: false
    // The window items to blur, top-level parts of an ApplicationWindow.
    property var sources: []

    anchors.fill: parent
    z: -1
    visible: opacity > 0
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: root.theme ? root.theme.durStandard : 0 } }

    Repeater {
        model: root.visible ? root.sources : []
        delegate: Item {
            id: part
            required property var modelData
            // Where the part is, re-read when the sheet opens and when the part resizes.
            readonly property point origin: (root.shown, modelData.width, modelData.height, root.width,
                                             modelData.mapToItem(root, 0, 0))
            x: origin.x
            y: origin.y
            width: modelData.width
            height: modelData.height
            ShaderEffectSource {
                id: capture
                anchors.fill: parent
                sourceItem: part.modelData
                live: true
                visible: false
            }
            MultiEffect {
                anchors.fill: parent
                source: capture
                autoPaddingEnabled: false
                blurEnabled: true
                blurMax: 40
                blur: 1.0
                saturation: -0.15
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.theme && root.theme.scrim !== undefined ? root.theme.scrim : "transparent"
    }
}
