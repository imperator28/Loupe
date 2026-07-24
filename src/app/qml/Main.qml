import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App
import "inspect" as Inspect
import "export" as Export

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
    property WindowChrome windowChrome: WindowChrome {}
    readonly property color themeForeground: appTheme.foreground
    readonly property color themeMuted: appTheme.muted

    function isSupportedStepUrl(fileUrl) {
        const value = String(fileUrl).toLowerCase().split(/[?#]/)[0]
        return value.startsWith("file:") && (value.endsWith(".step") || value.endsWith(".stp"))
    }

    function canOpenDroppedUrls(urls) {
        return urls && urls.length === 1 && isSupportedStepUrl(urls[0])
    }

    function openDroppedUrls(urls) {
        if (!canOpenDroppedUrls(urls)) return false
        controller.setWorkspace(AppState.Inspect)
        controller.openFile(urls[0])
        return true
    }

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    Theme {
        id: appTheme
        systemDark: systemPalette.window.r + systemPalette.window.g + systemPalette.window.b < 1.5
        // The OS does not re-skin native chrome (e.g. the macOS title bar)
        // when only the in-app theme changes, so that has to be driven
        // explicitly whenever the effective dark/light state changes.
        onDarkChanged: root.windowChrome.applyAppearance(root, dark)
    }

    Component.onCompleted: {
        appTheme.mode = themePreference.mode
        windowChrome.applyAppearance(root, appTheme.dark)
    }
    Connections {
        target: root.themePreference
        function onModeChanged() { appTheme.mode = root.themePreference.mode }
    }

    menuBar: Inspect.ThemedMenuBar {
        theme: appTheme
        Inspect.ThemedMenu {
            theme: appTheme
            title: qsTr("File")
            Inspect.ThemedMenuItem {
                theme: appTheme
                action: Action {
                    text: qsTr("Open STEP…")
                    shortcut: StandardKey.Open
                    onTriggered: openStepDialog.open()
                }
            }
            Inspect.ThemedMenuSeparator { theme: appTheme }
            Inspect.ThemedMenuItem {
                theme: appTheme
                action: Action {
                    text: qsTr("Quit Loupe")
                    shortcut: StandardKey.Quit
                    onTriggered: Qt.quit()
                }
            }
        }
        Inspect.ThemedMenu {
            theme: appTheme
            title: qsTr("View")
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("Inspect")
                checkable: true
                checked: root.controller.workspace === AppState.Inspect
                onTriggered: root.controller.setWorkspace(AppState.Inspect)
            }
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("Export")
                checkable: true
                checked: root.controller.workspace === AppState.Export
                onTriggered: root.controller.setWorkspace(AppState.Export)
            }
            Inspect.ThemedMenuSeparator { theme: appTheme }
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("Interaction…")
                onTriggered: interactionGuide.open()
            }
            Inspect.ThemedMenuSeparator { theme: appTheme }
            Inspect.ThemedMenu {
                theme: appTheme
                title: qsTr("Appearance")
                Inspect.ThemedMenuItem {
                    theme: appTheme
                    text: qsTr("System")
                    checkable: true
                    checked: root.themePreference.mode === "System"
                    onTriggered: { appTheme.mode = "System"; root.themePreference.mode = "System" }
                }
                Inspect.ThemedMenuItem {
                    theme: appTheme
                    text: qsTr("Light")
                    checkable: true
                    checked: root.themePreference.mode === "Light"
                    onTriggered: { appTheme.mode = "Light"; root.themePreference.mode = "Light" }
                }
                Inspect.ThemedMenuItem {
                    theme: appTheme
                    text: qsTr("Dark")
                    checkable: true
                    checked: root.themePreference.mode === "Dark"
                    onTriggered: { appTheme.mode = "Dark"; root.themePreference.mode = "Dark" }
                }
            }
            Inspect.ThemedMenuSeparator { theme: appTheme }
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("Reduce Motion")
                checkable: true
                checked: appTheme.reducedMotion
                onTriggered: appTheme.reducedMotion = checked
            }
            Inspect.ThemedMenuSeparator { theme: appTheme }
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("Review Source Units…")
                onTriggered: unitReview.open()
            }
        }
        Inspect.ThemedMenu {
            theme: appTheme
            title: qsTr("Help")
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("About Loupe")
                onTriggered: aboutLoupe.open()
            }
        }
    }

    header: ToolBar {
        implicitHeight: appTheme.toolbarHeight
        palette.buttonText: appTheme.onSurface
        palette.text: appTheme.onSurface
        background: Rectangle {
            color: appTheme.surfaceRaised
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: appTheme.border
            }
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: appTheme.spacing4
            anchors.rightMargin: appTheme.spacing4
            spacing: appTheme.spacing3

            Inspect.ThemedButton {
                text: qsTr("Open STEP…")
                theme: appTheme
                Accessible.name: qsTr("Open a STEP file")
                onClicked: openStepDialog.open()
            }
            Inspect.ThemedSegmentedControl {
                id: workspaceSwitcher
                theme: appTheme
                model: [qsTr("Inspect"), qsTr("Export")]
                currentIndex: root.controller.workspace === AppState.Inspect ? 0 : 1
                implicitWidth: 176
                Accessible.name: qsTr("Workspace")
                onActivated: index => root.controller.setWorkspace(index === 0 ? AppState.Inspect : AppState.Export)
            }
            Item { Layout.fillWidth: true }
            Inspect.ThemedButton {
                text: qsTr("Units: %1").arg(root.controller.effectiveUnit)
                theme: appTheme
                Accessible.name: qsTr("Review source units")
                onClicked: unitReview.open()
            }
            Inspect.ThemedComboBox {
                id: themeChooser
                theme: appTheme
                model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                currentIndex: root.themePreference.mode === "Light" ? 1 : root.themePreference.mode === "Dark" ? 2 : 0
                Accessible.name: qsTr("Color theme")
                implicitWidth: 116
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
        Export.ExportWorkspace {
            controller: root.controller
            theme: appTheme
        }
    }

    DropArea {
        id: fileDropArea
        objectName: "stepFileDropArea"
        anchors.fill: parent
        z: 200
        property bool supported: false

        onEntered: function(drag) {
            supported = root.canOpenDroppedUrls(drag.urls)
            drag.accepted = supported
        }
        onExited: supported = false
        onDropped: function(drop) {
            const opened = root.openDroppedUrls(drop.urls)
            supported = false
            if (opened) drop.acceptProposedAction()
        }
    }

    Item {
        id: fileDropOverlay
        objectName: "stepFileDropOverlay"
        anchors.fill: parent
        z: 199
        visible: fileDropArea.containsDrag
        enabled: false
        readonly property real cornerRadius: appTheme.windowRadius
        readonly property color borderColor: fileDropArea.supported ? appTheme.accent : appTheme.error

        // Keep the top edge straight under the toolbar, but begin the rounded
        // surface above the content area so its lower corners follow native
        // macOS/Windows window framing instead of ending as a square border.
        Rectangle {
            id: dropOverlaySurface
            objectName: "stepFileDropOverlaySurface"
            x: 0
            y: -fileDropOverlay.cornerRadius
            width: parent.width
            height: parent.height + fileDropOverlay.cornerRadius
            radius: fileDropOverlay.cornerRadius
            color: Qt.rgba(appTheme.surface.r, appTheme.surface.g, appTheme.surface.b, 0.9)
            border.width: 3
            border.color: fileDropOverlay.borderColor
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 3
            color: fileDropOverlay.borderColor
        }

        Column {
            anchors.centerIn: parent
            spacing: appTheme.spacing2
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: fileDropArea.supported ? qsTr("Drop STEP to open") : qsTr("Choose one .step or .stp file")
                color: fileDropArea.supported ? appTheme.accent : appTheme.error
                font.bold: true
                font.pixelSize: appTheme.fontTitle
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: fileDropArea.supported ? qsTr("Loupe will open it in the Inspect workspace")
                                             : qsTr("This drop cannot be opened")
                color: appTheme.muted
                font.pixelSize: appTheme.fontBody
            }
        }
    }

    Popup {
        id: unitReview
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(560, Overlay.overlay.width - appTheme.spacing6 * 2)
        padding: appTheme.spacing6
        background: Inspect.ElevatedPanel { theme: appTheme; cornerRadius: appTheme.radius4 }
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: appTheme.durStandard }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: appTheme.durStandard
                easing.type: Easing.BezierSpline; easing.bezierCurve: appTheme.easeEnter }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Math.round(appTheme.durStandard * 0.7) }
        }
        contentItem: ColumnLayout {
            width: unitReview.availableWidth
            spacing: appTheme.spacing3

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
                    primary: true
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

    Inspect.AboutLoupeDialog {
        id: aboutLoupe
        theme: appTheme
    }

    Inspect.InteractionGuide {
        id: interactionGuide
        theme: appTheme
    }

    OpenStepDialog {
        id: openStepDialog
        controller: root.controller
    }
}
