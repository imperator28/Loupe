import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A color picker built entirely from themed primitives, replacing
// QtQuick.Dialogs.ColorDialog. That built-in dialog exposes no palette hook
// and renders through Qt's own implementation, so it cannot be made to follow
// Loupe's runtime light/dark theme. This one binds every surface to appTheme
// and stays consistent with the rest of the app. API mirrors ColorDialog:
// set `selectedColor` before open(), read `selectedColor` in onAccepted.
Dialog {
    id: root

    property QtObject theme
    // Input: the caller binds this to the current color; read only to seed the
    // picker on open, never reassigned internally so the caller's binding stays
    // intact across repeated opens (otherwise a reopen would show a stale color).
    property color selectedColor: "#67d5c0"

    // HSV is the editing source of truth; pickedColor is the derived result the
    // caller reads in onAccepted. Editing in HSV means dragging never
    // round-trips through an RGB hex string (which loses precision and makes
    // the handles jitter).
    property real hueValue: 0
    property real satValue: 0
    property real valValue: 1
    readonly property color pickedColor: Qt.hsva(hueValue, satValue, valValue, 1)

    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color muted: theme ? theme.muted : "transparent"

    // 0 = Hex field, 1 = R/G/B spin boxes.
    property int inputMode: 0
    readonly property int rgbR: Math.round(pickedColor.r * 255)
    readonly property int rgbG: Math.round(pickedColor.g * 255)
    readonly property int rgbB: Math.round(pickedColor.b * 255)

    modal: true
    anchors.centerIn: Overlay.overlay
    width: 400
    padding: theme ? theme.spacing4 : 16

    palette.window: theme ? theme.surfaceRaised : "transparent"
    palette.windowText: foreground
    palette.base: theme ? theme.surface : "transparent"
    palette.text: foreground
    palette.button: theme ? theme.control : "transparent"
    palette.buttonText: foreground

    background: ElevatedPanel {
        theme: root.theme
        cornerRadius: root.theme ? root.theme.radius4 : 10
    }
    header: null

    function toHexByte(component) {
        const v = Math.max(0, Math.min(255, Math.round(component * 255)))
        const s = v.toString(16)
        return s.length < 2 ? "0" + s : s
    }
    function formatHex(color) {
        return "#" + toHexByte(color.r) + toHexByte(color.g) + toHexByte(color.b)
    }
    function syncHexField() {
        hexField.text = formatHex(root.pickedColor)
    }
    function applyPreset(hex) {
        const parsed = Qt.color(hex)
        root.hueValue = parsed.hsvHue >= 0 ? parsed.hsvHue : root.hueValue
        root.satValue = parsed.hsvSaturation
        root.valValue = parsed.hsvValue
        root.syncHexField()
    }
    function applyRgb(r, g, b) {
        const parsed = Qt.rgba(r / 255, g / 255, b / 255, 1)
        root.hueValue = parsed.hsvHue >= 0 ? parsed.hsvHue : root.hueValue
        root.satValue = parsed.hsvSaturation
        root.valValue = parsed.hsvValue
        root.syncHexField()
    }
    // Apple's published macOS/iOS system colors (Human Interface Guidelines /
    // UIKit-AppKit reference), not a pixel copy of NSColorPanel's "Standard"
    // swatch grid — that grid's exact values aren't something we can verify
    // without a live Mac, whereas these are Apple's own documented palette.
    readonly property var systemColorPresets: [
        "#FF3B30", "#FF9500", "#FFCC00", "#34C759", "#00C7BE",
        "#30B0C7", "#32ADE6", "#007AFF", "#5856D6", "#AF52DE",
        "#FF2D55", "#A2845E", "#8E8E93"
    ]
    // The classic macOS "Crayons" palette, by name. These names/values are the
    // widely-circulated reference list from Cocoa/AppleScript color-picker
    // documentation over the years — not something verifiable pixel-for-pixel
    // against a live NSColorPanel from here, but a high-confidence, stable
    // community reference, unlike a guess at the "Standard" tab's grid.
    readonly property var crayonPresets: [
        {"name": "Cayenne", "hex": "#990000"}, {"name": "Mocha", "hex": "#7f462c"},
        {"name": "Asparagus", "hex": "#808000"}, {"name": "Fern", "hex": "#4e9a06"},
        {"name": "Clover", "hex": "#339900"}, {"name": "Moss", "hex": "#4a5d23"},
        {"name": "Spruce", "hex": "#0f5c4b"}, {"name": "Ocean", "hex": "#0066cc"},
        {"name": "Eggplant", "hex": "#660066"}, {"name": "Plum", "hex": "#dda0dd"},
        {"name": "Maraschino", "hex": "#ff2600"}, {"name": "Tangerine", "hex": "#ff7f00"},
        {"name": "Lemon", "hex": "#fffe00"}, {"name": "Lime", "hex": "#40ff00"},
        {"name": "Sea Foam", "hex": "#66ffb2"}, {"name": "Turquoise", "hex": "#00ffcc"},
        {"name": "Sky", "hex": "#56b4e3"}, {"name": "Grape", "hex": "#660099"},
        {"name": "Magenta", "hex": "#cc0099"}, {"name": "Carnation", "hex": "#ff6699"},
        {"name": "Salmon", "hex": "#ff8c69"}, {"name": "Cantaloupe", "hex": "#ffcc66"},
        {"name": "Banana", "hex": "#ffff66"}, {"name": "Honeydew", "hex": "#e6ffcc"},
        {"name": "Flora", "hex": "#66ff66"}, {"name": "Spindrift", "hex": "#66ffcc"},
        {"name": "Ice", "hex": "#b5f4e0"}, {"name": "Orchid", "hex": "#c3a0de"},
        {"name": "Violet", "hex": "#8000ff"}, {"name": "Lavender", "hex": "#cc99ff"},
        {"name": "Licorice", "hex": "#000000"}, {"name": "Lead", "hex": "#333333"},
        {"name": "Tungsten", "hex": "#666666"}, {"name": "Iron", "hex": "#808080"},
        {"name": "Steel", "hex": "#999999"}, {"name": "Aluminum", "hex": "#b3b3b3"},
        {"name": "Magnesium", "hex": "#cccccc"}, {"name": "Silver", "hex": "#dddddd"},
        {"name": "Mercury", "hex": "#eeeeee"}, {"name": "Snow", "hex": "#ffffff"}
    ]
    readonly property int crayonsPerRow: 10
    function crayonRowCount() {
        return Math.ceil(crayonPresets.length / crayonsPerRow)
    }
    function crayonRowItems(rowIndex) {
        return crayonPresets.slice(rowIndex * crayonsPerRow, rowIndex * crayonsPerRow + crayonsPerRow)
    }
    // Seed the HSV editing state from selectedColor when the dialog opens. Qt's
    // color exposes hsvHue/hsvSaturation/hsvValue; hue is -1 for achromatic
    // colors, so fall back to the current hue to keep the slider put.
    function loadFromSelectedColor() {
        const c = root.selectedColor
        root.hueValue = c.hsvHue >= 0 ? c.hsvHue : root.hueValue
        root.satValue = c.hsvSaturation
        root.valValue = c.hsvValue
        root.syncHexField()
    }

    onAboutToShow: root.loadFromSelectedColor()

    contentItem: ColumnLayout {
        spacing: root.theme ? root.theme.spacing3 : 12

        Label {
            text: root.title
            visible: root.title.length > 0
            color: root.foreground
            font.bold: true
            font.pixelSize: root.theme ? root.theme.fontTitle : 15
            Layout.fillWidth: true
        }

        // Saturation / value square.
        Item {
            id: svArea
            Layout.fillWidth: true
            Layout.preferredHeight: 200

            Rectangle {
                id: svSquare
                anchors.fill: parent
                radius: root.theme ? root.theme.radius2 : 6
                color: Qt.hsva(root.hueValue, 1, 1, 1)

                // white (left, full) -> transparent (right): saturation
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "#ffffffff" }
                        GradientStop { position: 1; color: "#00ffffff" }
                    }
                }
                // transparent (top) -> black (bottom): value
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0; color: "#00000000" }
                        GradientStop { position: 1; color: "#ff000000" }
                    }
                }

                // Handle.
                Rectangle {
                    width: 14
                    height: 14
                    radius: 7
                    border.color: "#ffffff"
                    border.width: 2
                    color: "transparent"
                    x: root.satValue * (svSquare.width) - width / 2
                    y: (1 - root.valValue) * (svSquare.height) - height / 2
                    Rectangle {
                        anchors.centerIn: parent
                        width: 12; height: 12; radius: 6
                        color: "transparent"
                        border.color: "#80000000"
                        border.width: 1
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    function apply(mx, my) {
                        root.satValue = Math.max(0, Math.min(1, mx / svSquare.width))
                        root.valValue = Math.max(0, Math.min(1, 1 - my / svSquare.height))
                        root.syncHexField()
                    }
                    onPressed: mouse => apply(mouse.x, mouse.y)
                    onPositionChanged: mouse => apply(mouse.x, mouse.y)
                }
            }
        }

        // Hue slider.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 18

            Rectangle {
                id: hueTrack
                anchors.fill: parent
                radius: height / 2
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.000; color: "#ff0000" }
                    GradientStop { position: 0.166; color: "#ffff00" }
                    GradientStop { position: 0.333; color: "#00ff00" }
                    GradientStop { position: 0.500; color: "#00ffff" }
                    GradientStop { position: 0.666; color: "#0000ff" }
                    GradientStop { position: 0.833; color: "#ff00ff" }
                    GradientStop { position: 1.000; color: "#ff0000" }
                }

                Rectangle {
                    width: 6
                    height: parent.height + 6
                    radius: 3
                    y: -3
                    x: root.hueValue * parent.width - width / 2
                    color: "#ffffff"
                    border.color: "#80000000"
                    border.width: 1
                }

                MouseArea {
                    anchors.fill: parent
                    function apply(mx) {
                        root.hueValue = Math.max(0, Math.min(1, mx / hueTrack.width))
                        root.syncHexField()
                    }
                    onPressed: mouse => apply(mouse.x)
                    onPositionChanged: mouse => apply(mouse.x)
                }
            }
        }

        // macOS system color presets: one-click common swatches.
        Label {
            text: qsTr("System Colors")
            color: root.muted
            font.pixelSize: root.theme ? root.theme.fontCaption : 11
            Layout.fillWidth: true
        }
        Flow {
            Layout.fillWidth: true
            spacing: root.theme ? root.theme.spacing1 : 4

            Repeater {
                model: root.systemColorPresets
                delegate: Rectangle {
                    required property string modelData
                    width: 24
                    height: 24
                    radius: width / 2
                    color: modelData
                    border.color: root.theme ? root.theme.border : "#00000000"
                    border.width: 1

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.applyPreset(parent.modelData)
                    }
                }
            }
        }

        // The macOS Crayons palette: a staggered grid (alternating row inset)
        // evoking the look of the real Crayons picker, rather than a plain grid.
        Label {
            text: qsTr("Crayons")
            color: root.muted
            font.pixelSize: root.theme ? root.theme.fontCaption : 11
            Layout.fillWidth: true
        }
        Column {
            Layout.fillWidth: true
            spacing: root.theme ? root.theme.spacing1 : 4

            Repeater {
                model: root.crayonRowCount()
                delegate: Row {
                    id: crayonRow
                    required property int index
                    // Center each row in the column, staggering alternate rows
                    // left/right of center rather than offsetting from the left
                    // edge — keeps the whole block visually centered.
                    x: (parent.width - width) / 2 + (index % 2 === 0 ? -5.5 : 5.5)
                    spacing: root.theme ? root.theme.spacing1 : 4

                    Repeater {
                        model: root.crayonRowItems(crayonRow.index)
                        delegate: Rectangle {
                            required property var modelData
                            width: 22
                            height: 22
                            radius: width / 2
                            color: modelData.hex
                            border.color: root.theme ? root.theme.border : "#00000000"
                            border.width: 1

                            MouseArea {
                                id: crayonArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: root.applyPreset(parent.modelData.hex)
                            }
                            ToolTip.visible: crayonArea.containsMouse
                            ToolTip.text: modelData.name
                        }
                    }
                }
            }
        }

        // Preview swatch + Hex/RGB input, with a mode toggle between the two.
        RowLayout {
            Layout.fillWidth: true
            spacing: root.theme ? root.theme.spacing2 : 8

            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: root.theme ? root.theme.radius2 : 6
                color: root.pickedColor
                border.color: root.theme ? root.theme.border : "transparent"
                border.width: 1
            }

            ThemedSegmentedControl {
                theme: root.theme
                model: [qsTr("Hex"), qsTr("RGB")]
                currentIndex: root.inputMode
                onActivated: index => root.inputMode = index
            }

            ThemedTextField {
                id: hexField
                theme: root.theme
                visible: root.inputMode === 0
                Layout.fillWidth: true
                text: root.formatHex(root.pickedColor)
                maximumLength: 7
                onEditingFinished: {
                    const normalized = text.charAt(0) === "#" ? text : "#" + text
                    if (/^#[0-9a-fA-F]{6}$/.test(normalized)) {
                        const parsed = Qt.color(normalized)
                        root.hueValue = parsed.hsvHue >= 0 ? parsed.hsvHue : root.hueValue
                        root.satValue = parsed.hsvSaturation
                        root.valValue = parsed.hsvValue
                    }
                    // Re-format to the canonical value (repairs partial/invalid input).
                    root.syncHexField()
                }
            }

            RowLayout {
                visible: root.inputMode === 1
                Layout.fillWidth: true
                spacing: root.theme ? root.theme.spacing1 : 4

                ThemedSpinBox {
                    id: rSpin
                    theme: root.theme
                    Layout.fillWidth: true
                    from: 0; to: 255
                    value: root.rgbR
                    onValueModified: root.applyRgb(value, gSpin.value, bSpin.value)
                }
                ThemedSpinBox {
                    id: gSpin
                    theme: root.theme
                    Layout.fillWidth: true
                    from: 0; to: 255
                    value: root.rgbG
                    onValueModified: root.applyRgb(rSpin.value, value, bSpin.value)
                }
                ThemedSpinBox {
                    id: bSpin
                    theme: root.theme
                    Layout.fillWidth: true
                    from: 0; to: 255
                    value: root.rgbB
                    onValueModified: root.applyRgb(rSpin.value, gSpin.value, value)
                }
            }
        }
    }

    footer: Item {
        implicitHeight: footerRow.implicitHeight + (root.theme ? root.theme.spacing4 * 2 : 32)
        RowLayout {
            id: footerRow
            anchors.fill: parent
            anchors.margins: root.theme ? root.theme.spacing4 : 16
            spacing: root.theme ? root.theme.spacing2 : 8

            Item { Layout.fillWidth: true }
            ThemedButton {
                theme: root.theme
                text: qsTr("Cancel")
                onClicked: root.reject()
            }
            ThemedButton {
                theme: root.theme
                primary: true
                text: qsTr("OK")
                onClicked: root.accept()
            }
        }
    }
}
