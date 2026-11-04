import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App

ElevatedPanel {
    id: root
    signal toolTriggered(string tool)
    property int viewerPresentation: AppState.Full
    // Parts are out of place in the exploded view (#17): its button stays lit, and measuring,
    // which works on the assembled model, waits until they are reset.
    property bool exploded: false
    property bool measureBlocked: false
    // The whole selection is transparent: the button stays lit and turns it opaque again.
    property bool selectionTransparent: false
    property var tools: [
        { key: "fit", icon: "fit", label: qsTr("Fit") },
        { key: "isolate", icon: "isolate", label: qsTr("Isolate"), active: viewerPresentation === AppState.Isolate },
        { key: "hide", icon: "hide", label: qsTr("Hide") },
        { key: "transparent", icon: "transparent", label: qsTr("Transparent"), active: selectionTransparent },
        { key: "section", icon: "section", label: qsTr("Section") },
        { key: "explode", icon: "explode", label: qsTr("Explode"), active: exploded },
        { key: "measure", icon: "measure", label: qsTr("Measure"), disabled: measureBlocked,
          hint: qsTr("Measuring works on the assembled model: switch the exploded view to Assembled to measure.") },
        { key: "views", icon: "views", label: qsTr("Views") },
        { key: "capture", icon: "capture", label: qsTr("Capture") }
    ]
    flat: true
    cornerRadius: theme.radius2
    implicitWidth: controls.implicitWidth + 12
    implicitHeight: 62

    HoverHandler { id: dockHover }

    RowLayout {
        id: controls
        anchors.fill: parent
        anchors.margins: root.theme.spacing1 + 2
        spacing: root.theme.spacing1
        Repeater {
            model: root.tools
            delegate: ThemedToolButton {
                id: toolButton
                required property var modelData
                theme: root.theme
                checkable: modelData.key === "isolate"
                checked: modelData.active === true
                enabled: modelData.disabled !== true
                palette.buttonText: root.theme.onSurface
                palette.text: root.theme.onSurface
                Layout.minimumWidth: 52
                Layout.minimumHeight: 52
                Accessible.name: modelData.label
                onClicked: root.toolTriggered(modelData.key)
                // A disabled button takes no hover of its own, so the dock's says when the
                // pointer is over it, to show why it is off.
                ThemedToolTip {
                    theme: root.theme
                    text: toolButton.modelData.hint !== undefined ? toolButton.modelData.hint : ""
                    visible: !toolButton.enabled && text.length > 0 && dockHover.hovered
                             && toolButton.contains(toolButton.mapFromItem(root, dockHover.point.position))
                }

                // Control owns contentItem's own geometry directly (it
                // stretches it to fill the padding box via internal C++
                // bindings, which silently wins over anchors set on that
                // same item). So contentItem stays a plain filling Item,
                // and the actual Column is its *child* — free to center
                // itself since Control never touches a grandchild's geometry.
                contentItem: Item {
                    implicitWidth: contentColumn.implicitWidth
                    implicitHeight: contentColumn.implicitHeight

                    Column {
                        id: contentColumn
                        anchors.centerIn: parent
                        spacing: 3

                        ToolIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            name: toolButton.modelData.icon
                            color: toolButton.enabled ? toolButton.foreground : toolButton.subduedForeground
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: toolButton.modelData.label
                            color: toolButton.subduedForeground
                            font.pixelSize: root.theme.fontCaption - 1
                        }
                    }
                }
            }
        }
    }
}
