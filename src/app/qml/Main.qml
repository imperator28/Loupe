import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App
import "inspect" as Inspect

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    visible: true
    title: qsTr("Loupe")
    color: appTheme.surface
    palette.window: appTheme.surface
    palette.windowText: appTheme.onSurface
    palette.base: appTheme.surfaceRaised
    palette.alternateBase: appTheme.surfaceSubtle
    palette.text: appTheme.onSurface
    palette.button: appTheme.control
    palette.buttonText: appTheme.onSurface
    palette.highlight: appTheme.selection
    palette.highlightedText: appTheme.onSurface
    palette.placeholderText: appTheme.muted
    palette.brightText: appTheme.onSurface
    palette.light: appTheme.border
    palette.mid: appTheme.border
    palette.dark: appTheme.surface
    palette.link: appTheme.accent
    palette.toolTipBase: appTheme.surfaceRaised
    palette.toolTipText: appTheme.onSurface
    property ApplicationController controller: ApplicationController {}
    property ThemePreference themePreference: ThemePreference {}
    readonly property color themeForeground: appTheme.dark ? "#e6edf3" : "#172127"
    readonly property color themeMuted: appTheme.dark ? "#aeb8c2" : "#53636c"

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    QtObject {
        id: appTheme
        property string mode: "System"
        readonly property bool dark: mode === "Dark" || (mode === "System"
            && systemPalette.window.r + systemPalette.window.g + systemPalette.window.b < 1.5)

        readonly property color surface: dark ? "#101418" : "#f5f7f8"
        readonly property color viewport: dark ? "#101418" : "#f7f9fa"
        readonly property color surfaceRaised: dark ? "#182027" : "#ffffff"
        readonly property color surfaceSubtle: dark ? "#202a33" : "#e8edf0"
        readonly property color control: dark ? "#26323c" : "#dde5e9"
        readonly property color border: dark ? "#34414b" : "#b9c5cb"
        readonly property color onSurface: dark ? "#e6edf3" : "#172127"
        readonly property color muted: dark ? "#aeb8c2" : "#53636c"
        readonly property color accent: dark ? "#67d5c0" : "#087b74"
        readonly property color selection: dark ? "#315b64" : "#a8ddd7"
        readonly property color selectedBody: dark ? "#ffd166" : "#a86100"
        readonly property color selectedEdge: dark ? "#fff0a6" : "#7d4a00"
        readonly property color edge: dark ? "#b8c7d1" : "#3a525d"
        readonly property color error: dark ? "#ffb4ab" : "#b42318"
    }

    Component.onCompleted: appTheme.mode = themePreference.mode
    Connections {
        target: root.themePreference
        function onModeChanged() { appTheme.mode = root.themePreference.mode }
    }

    header: ToolBar {
        palette.buttonText: appTheme.onSurface
        palette.text: appTheme.onSurface
        background: Rectangle { color: appTheme.surfaceRaised }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            Label {
                text: qsTr("Loupe")
                font.bold: true
                color: appTheme.dark ? "#e6edf3" : "#172127"
            }
            Inspect.ThemedButton {
                text: qsTr("Open STEP…")
                theme: appTheme
                Accessible.name: qsTr("Open a STEP file")
                onClicked: openStepDialog.open()
            }
            TabBar {
                id: workspaceSwitcher
                currentIndex: root.controller.workspace === AppState.Inspect ? 0 : 1
                onCurrentIndexChanged: root.controller.setWorkspace(currentIndex === 0 ? AppState.Inspect : AppState.Export)
                Inspect.ThemedTabButton { text: qsTr("Inspect"); theme: appTheme }
                Inspect.ThemedTabButton { text: qsTr("Export"); theme: appTheme }
            }
            Item { Layout.fillWidth: true }
            Inspect.ThemedButton {
                text: qsTr("Units: %1").arg(root.controller.effectiveUnit)
                theme: appTheme
                Accessible.name: qsTr("Review source units")
                onClicked: unitReview.open()
            }
            ComboBox {
                id: themeChooser
                model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                currentIndex: root.themePreference.mode === "Light" ? 1 : root.themePreference.mode === "Dark" ? 2 : 0
                Accessible.name: qsTr("Color theme")
                implicitWidth: 116
                implicitHeight: 30
                leftPadding: 10
                rightPadding: 28
                contentItem: Label {
                    text: themeChooser.displayText
                    color: root.themeForeground
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                background: Rectangle {
                    radius: 4
                    color: appTheme.control
                    border.color: appTheme.border
                }
                indicator: Text {
                    text: "⌄"
                    color: root.themeForeground
                    anchors.right: parent.right
                    anchors.rightMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                }
                delegate: ItemDelegate {
                    width: themeChooser.width
                    height: 30
                    highlighted: themeChooser.highlightedIndex === index
                    contentItem: Label {
                        text: modelData
                        color: root.themeForeground
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }
                    background: Rectangle {
                        color: parent.highlighted ? appTheme.selection : appTheme.surfaceRaised
                    }
                }
                popup: Popup {
                    y: themeChooser.height + 4
                    width: themeChooser.width
                    implicitHeight: contentItem.implicitHeight + 8
                    padding: 4
                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: themeChooser.popup.visible ? themeChooser.delegateModel : null
                        currentIndex: themeChooser.highlightedIndex
                    }
                    background: Rectangle {
                        radius: 4
                        color: appTheme.surfaceRaised
                        border.color: appTheme.border
                    }
                }
                onActivated: {
                    const mode = currentIndex === 1 ? "Light" : currentIndex === 2 ? "Dark" : "System"
                    appTheme.mode = mode
                    root.themePreference.mode = mode
                }
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.controller.workspace === AppState.Inspect ? 0 : 1

        Inspect.InspectWorkspace {
            controller: root.controller
            theme: appTheme
            onOpenFileRequested: openStepDialog.open()
        }
        Item {
            Label {
                anchors.centerIn: parent
                text: qsTr("Export workspace")
            }
        }
    }

    Popup {
        id: unitReview
        modal: true
        anchors.centerIn: Overlay.overlay
        padding: 20
        contentItem: ColumnLayout {
            width: 300
            spacing: 12

            Label {
                text: qsTr("Source unit review")
                font.bold: true
                font.pixelSize: 16
                color: root.themeForeground
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("Current interpretation: %1. Changing it reimports this file, rescales geometry analysis, measurements, and mass estimates, and records the choice for this exact source file.").arg(root.controller.effectiveUnit)
                color: root.themeForeground
            }
            Label {
                text: qsTr("Largest analyzed extent: %1 mm").arg(root.controller.modelExtentMm.toFixed(3))
                color: root.themeMuted
            }
            RowLayout {
                Layout.fillWidth: true
            Inspect.ThemedButton {
                    theme: appTheme
                    Layout.fillWidth: true
                    text: qsTr("Interpret as mm")
                    enabled: root.controller.documentState === AppState.TreeReady
                    onClicked: {
                        if (root.controller.setUnitOverride("mm")) unitReview.close()
                    }
                }
                Inspect.ThemedButton {
                    theme: appTheme
                    Layout.fillWidth: true
                    text: qsTr("Interpret as inches")
                    enabled: root.controller.documentState === AppState.TreeReady
                    onClicked: {
                        if (root.controller.setUnitOverride("in")) unitReview.close()
                    }
                }
            }
        }
    }

    OpenStepDialog {
        id: openStepDialog
        controller: root.controller
    }
}
