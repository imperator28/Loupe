import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App

// The Inspect toolbar, in two groups split by a rule:
//   on the model -- Fit, and Isolate, Hide, Transparent acting on the selection, at once;
//   tool windows -- Section, Explode, Measure, Views, Capture, each opening its window,
//                   lit while that window is open.
// When the viewport is too narrow for the labels, the buttons drop to icons, each named in
// its tooltip, so the dock never runs past the viewport onto the side panels.
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
    // The tool whose window is open, or "".
    property string openTool: ""
    // The width the dock may take; past it, the buttons drop their labels.
    property real availableWidth: Number.POSITIVE_INFINITY

    readonly property var modelTools: [
        { key: "fit", icon: "fit", label: qsTr("Fit") },
        { key: "isolate", icon: "isolate", label: qsTr("Isolate"), active: viewerPresentation === AppState.Isolate },
        { key: "hide", icon: "hide", label: qsTr("Hide") },
        { key: "transparent", icon: "transparent", label: qsTr("Transparent"), active: selectionTransparent }
    ]
    readonly property var windowTools: [
        { key: "section", icon: "section", label: qsTr("Section"), active: openTool === "section" },
        { key: "explode", icon: "explode", label: qsTr("Explode"), active: exploded || openTool === "explode" },
        { key: "measure", icon: "measure", label: qsTr("Measure"), active: openTool === "measure", disabled: measureBlocked,
          hint: qsTr("Measuring works on the assembled model: switch the exploded view to Assembled to measure.") },
        { key: "views", icon: "views", label: qsTr("Views"), active: openTool === "views" },
        { key: "capture", icon: "capture", label: qsTr("Capture"), active: openTool === "capture" }
    ]
    readonly property var tools: modelTools.concat(windowTools)

    readonly property int labelledButtonWidth: 52
    readonly property int compactButtonSize: 34
    readonly property int dockPadding: theme.spacing1 + 2
    readonly property int dividerGap: theme.spacing2
    function labelledWidth(label) {
        return Math.max(labelledButtonWidth, Math.ceil(labelMetrics.advanceWidth(label)) + 16)
    }
    // Measured from the labels, not from the laid-out row, so choosing compact never feeds
    // back into the width it is chosen from.
    readonly property real fullWidth: {
        let width = 2 * dockPadding + (tools.length - 2) * theme.spacing1 + 2 * dividerGap + 1
        for (const tool of tools) width += labelledWidth(tool.label)
        return width
    }
    readonly property bool compact: fullWidth > availableWidth
    readonly property int buttonSpacing: compact ? 2 : theme.spacing1

    flat: true
    cornerRadius: theme.radius2
    implicitWidth: controls.implicitWidth + 2 * dockPadding
    implicitHeight: compact ? compactButtonSize + 2 * dockPadding : 62

    FontMetrics {
        id: labelMetrics
        font.pixelSize: root.theme.fontCaption - 1
    }
    HoverHandler { id: dockHover }

    Component {
        id: dockButton
        ThemedToolButton {
            id: toolButton
            required property var modelData
            theme: root.theme
            checkable: modelData.key === "isolate"
            checked: modelData.active === true
            enabled: modelData.disabled !== true
            palette.buttonText: root.theme.onSurface
            palette.text: root.theme.onSurface
            Layout.preferredWidth: root.compact ? root.compactButtonSize : root.labelledWidth(modelData.label)
            Layout.preferredHeight: root.compact ? root.compactButtonSize : 52
            objectName: "dock-" + modelData.key
            Accessible.name: modelData.label
            onClicked: root.toolTriggered(modelData.key)
            // A disabled button takes no hover of its own, so the dock's says when the pointer is
            // over it, to show why it is off. Compact, the tooltip also names the button.
            readonly property bool pointerOver: dockHover.hovered
                && toolButton.contains(toolButton.mapFromItem(root, dockHover.point.position))
            ThemedToolTip {
                theme: root.theme
                text: !toolButton.enabled && toolButton.modelData.hint !== undefined ? toolButton.modelData.hint
                    : root.compact ? toolButton.modelData.label : ""
                visible: text.length > 0 && toolButton.pointerOver
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
                        visible: !root.compact
                        text: toolButton.modelData.label
                        color: toolButton.subduedForeground
                        font: labelMetrics.font
                    }
                }
            }
        }
    }

    RowLayout {
        id: controls
        anchors.centerIn: parent
        spacing: root.buttonSpacing

        Repeater {
            model: root.modelTools
            delegate: dockButton
        }
        // The rule between acting on the model and opening a tool window.
        Rectangle {
            objectName: "dockDivider"
            Layout.preferredWidth: 1
            Layout.preferredHeight: root.compact ? root.compactButtonSize - 10 : 32
            Layout.leftMargin: root.dividerGap - root.buttonSpacing
            Layout.rightMargin: root.dividerGap - root.buttonSpacing
            color: root.theme.border
        }
        Repeater {
            model: root.windowTools
            delegate: dockButton
        }
    }
}
