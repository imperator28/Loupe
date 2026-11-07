import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import Loupe.App

// The Inspect toolbar: a compact bar floating at the foot of the viewport, each tool a
// small line icon over a short label, in two groups split by a rule:
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
        { key: "fit", icon: "toolbarFit", label: qsTr("Fit") },
        { key: "isolate", icon: "toolbarIsolate", label: qsTr("Isolate"), active: viewerPresentation === AppState.Isolate },
        { key: "hide", icon: "toolbarHide", label: qsTr("Hide") },
        { key: "transparent", icon: "toolbarTransparent", label: qsTr("Transparent"), active: selectionTransparent }
    ]
    readonly property var windowTools: [
        { key: "section", icon: "toolbarSection", label: qsTr("Section"), active: openTool === "section" },
        { key: "explode", icon: "toolbarExplode", label: qsTr("Explode"), active: exploded || openTool === "explode" },
        { key: "measure", icon: "toolbarMeasure", label: qsTr("Measure"), active: openTool === "measure", disabled: measureBlocked,
          hint: qsTr("Measuring works on the assembled model: switch the exploded view to Assembled to measure.") },
        { key: "views", icon: "toolbarViews", label: qsTr("Views"), active: openTool === "views" },
        { key: "capture", icon: "toolbarCapture", label: qsTr("Capture"), active: openTool === "capture" }
    ]
    readonly property var tools: modelTools.concat(windowTools)

    // Geometry, from the landing page's drawing of this bar (site/v2/v2.css, .toolbar).
    readonly property int barPaddingV: 5
    readonly property int barPaddingH: 6
    readonly property int buttonSpacing: 1
    readonly property int buttonPaddingV: 4
    readonly property int buttonPaddingH: 6
    readonly property int iconSize: 17
    readonly property int iconLabelGap: 3
    readonly property int dividerInset: 4
    readonly property int dividerGap: 3
    // 10.5 px labels. A pixel size must be whole, so ask in points for the screen's DPI.
    readonly property real labelPixelSize: 10.5
    readonly property real logicalDpi: Screen.logicalPixelDensity > 0 ? Screen.logicalPixelDensity * 25.4
                                       : (Qt.platform.os === "osx" || Qt.platform.os === "macos" ? 72 : 96)
    // The stroke drawn at the toolbar's icon size: 1.6 in the icons' 24 px box.
    readonly property real iconStroke: 1.6 * iconSize / 24

    readonly property int labelHeight: Math.ceil(labelMetrics.height)
    readonly property int labelledButtonHeight: 2 * buttonPaddingV + iconSize + iconLabelGap + labelHeight
    readonly property int compactButtonHeight: 2 * buttonPaddingV + iconSize
    readonly property int compactButtonWidth: 2 * buttonPaddingH + iconSize
    function labelledWidth(label) {
        return Math.max(iconSize, Math.ceil(labelMetrics.advanceWidth(label))) + 2 * buttonPaddingH
    }
    // Every tool is as wide as the widest label needs, so the buttons sit evenly. Measured
    // from the labels, not from the laid-out row, so choosing compact never feeds back into
    // the width it is chosen from.
    readonly property int labelledButtonWidth: {
        let widest = 0
        for (const tool of tools) widest = Math.max(widest, labelledWidth(tool.label))
        return widest
    }
    readonly property real fullWidth: 2 * barPaddingH + tools.length * (labelledButtonWidth + buttonSpacing)
                                      + 2 * dividerGap + 1
    readonly property bool compact: fullWidth > availableWidth
    readonly property int buttonHeight: compact ? compactButtonHeight : labelledButtonHeight

    flat: true
    cornerRadius: 9
    color: theme.viewportToolbarFill
    border.color: theme.viewportToolbarHairline
    implicitWidth: controls.implicitWidth + 2 * barPaddingH
    implicitHeight: buttonHeight + 2 * barPaddingV

    // A soft shadow under the bar (CSS: 0 10px 24px -6px), drawn beneath it.
    RectangularShadow {
        z: -1
        anchors.fill: parent
        radius: root.cornerRadius
        offset.y: 10
        blur: 24
        spread: -6
        color: root.theme.viewportToolbarShadow
    }

    FontMetrics {
        id: labelMetrics
        font.pointSize: root.labelPixelSize * 72 / root.logicalDpi
    }
    HoverHandler { id: dockHover }

    Component {
        id: dockButton
        ThemedToolButton {
            id: toolButton
            required property var modelData
            readonly property bool lit: modelData.active === true
            theme: root.theme
            checkable: modelData.key === "isolate"
            checked: lit
            enabled: modelData.disabled !== true
            checkedFill: root.theme.viewportToolbarActive
            palette.buttonText: root.theme.onSurface
            palette.text: root.theme.onSurface
            implicitHeight: root.buttonHeight
            Layout.preferredWidth: root.compact ? root.compactButtonWidth : root.labelledButtonWidth
            Layout.preferredHeight: root.buttonHeight
            leftPadding: root.buttonPaddingH
            rightPadding: root.buttonPaddingH
            topPadding: root.buttonPaddingV
            bottomPadding: root.buttonPaddingV
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
                // Off: dimmed, and its tooltip says why.
                opacity: toolButton.enabled ? 1 : 0.45

                Column {
                    id: contentColumn
                    anchors.centerIn: parent
                    spacing: root.iconLabelGap

                    ToolIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: root.iconSize
                        height: root.iconSize
                        strokeWidth: root.iconStroke
                        name: toolButton.modelData.icon
                        color: toolButton.lit ? root.theme.accent : toolButton.subduedForeground
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: !root.compact
                        text: toolButton.modelData.label
                        color: toolButton.lit ? toolButton.foreground : toolButton.subduedForeground
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
            Layout.preferredHeight: root.buttonHeight - 2 * root.dividerInset
            Layout.leftMargin: root.dividerGap
            Layout.rightMargin: root.dividerGap
            color: root.theme.border
        }
        Repeater {
            model: root.windowTools
            delegate: dockButton
        }
    }
}
