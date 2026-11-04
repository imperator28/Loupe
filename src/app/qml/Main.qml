import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Loupe.App
import "inspect" as Inspect
import "export" as Export
import "drawing" as Drawing

ApplicationWindow {
    id: root

    // A single ordered list of workspaces. The switcher, the View menu and the
    // StackLayout all read it, so a fourth workspace cannot be added to one and forgotten
    // in the others -- which is exactly what the previous two-way ternaries invited.
    readonly property var workspaces: [
        { state: AppState.Inspect, label: qsTr("Inspect") },
        { state: AppState.Export, label: qsTr("Export") },
        { state: AppState.Drawing, label: qsTr("Drawing") }
    ]
    function workspaceIndexOf(document) {
        for (let index = 0; index < root.workspaces.length; ++index)
            if (document && root.workspaces[index].state === document.workspace) return index
        return 0
    }
    readonly property int workspaceIndex: workspaceIndexOf(root.controller)
    width: 1280
    height: 800
    visible: true
    title: root.controller.documentName.length > 0 ? qsTr("%1 — Loupe").arg(root.controller.documentName) : qsTr("Loupe")
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
    // One document per tab (#9). The menus, toolbar and dialogs act on the active one.
    property DocumentTabs documents: DocumentTabs {}
    readonly property ApplicationController controller: documents.active
    property ThemePreference themePreference: ThemePreference {}
    property WindowChrome windowChrome: WindowChrome {}
    property FileAssociation fileAssociation: FileAssociation {}
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
        root.openStepFile(urls[0])
        return true
    }

    // Into the current tab if nothing is open there, else a new tab.
    function openStepFile(fileUrl) {
        const document = root.documents.documentForOpening()
        document.setWorkspace(AppState.Inspect)
        document.openFile(fileUrl)
        return document
    }

    // Closing a tab mid-export would cancel the export, so that asks first.
    function requestCloseTab(index) {
        if (root.documents.isBusy(index)) {
            closeBusyTab.tabIndex = index
            closeBusyTab.open()
            return
        }
        root.documents.close(index)
    }

    // Export the assembly without its hidden parts (#7): hide what should go (search, Hide,
    // Isolate), then save the rest as one STEP file with its tree, names and colours.
    function chooseAssemblyExport() {
        if (root.controller.suggestedAssemblyExportUrl.toString().length > 0) {
            assemblyExportDialog.currentFolder = root.controller.suggestedAssemblyExportFolderUrl
            assemblyExportDialog.selectedFile = root.controller.suggestedAssemblyExportUrl
        }
        assemblyExportDialog.open()
    }

    // A new document starts from the remembered appearance: the last display style chosen
    // anywhere (#13), and the part colours.
    function applyNewDocumentPreferences(document) {
        document.renderMode = themePreference.renderMode
        applyPartColorPreferences(document)
    }

    function applyPartColorPreferences(document) {
        document.partColorMode = themePreference.partColorMode
        document.recolorFileColors = themePreference.recolorFileColors
        document.partColorSeed = themePreference.partColorSeed
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

    // Set when this window is opened for a tab dragged out of another one (#15).
    property QtObject adoptDocument: null

    // Where a tab dragged from another window would land if let go at global (x, y): its
    // index in this window's tabs, or -1 when the point is not over this window's header
    // (its toolbar and tab strip: a forgiving target, and there whether or not the strip is).
    function tabDropIndexAt(x, y) {
        if (!root.visible) return -1
        const area = root.header
        const local = area.mapFromGlobal(x, y)
        const slack = 8
        if (local.x < -slack || local.y < -slack || local.x > area.width + slack || local.y > area.height + slack) return -1
        const inRow = tabRow.mapFromGlobal(x, y)
        for (let i = 0; i < root.documents.count; ++i) {
            const tab = tabRepeater.itemAt(i)
            if (tab && inRow.x < tab.x + tab.width / 2) return i
        }
        return root.documents.count
    }

    // While a tab from another window is dragged over this one: mark where it would land.
    function showTabDropIndicator(x, y) {
        const index = tabDropIndexAt(x, y)
        if (index < 0) { hideTabDropIndicator(); return }
        tabDropIndicator.index = index
        tabDropIndicator.shown = true
    }

    function hideTabDropIndicator() {
        tabDropIndicator.shown = false
    }

    // Take in a tab dragged here from another window.
    function adoptDocumentAt(document, index) {
        hideTabDropIndicator()
        root.documents.adopt(document, index)
        root.raise()
        root.requestActivate()
    }

    // Closing a window closes its tabs, stopping their workers; the window itself is
    // destroyed by the window manager (or is the last one, and the app quits).
    onClosing: {
        while (root.documents.count > 1) root.documents.close(0)
        root.documents.close(0)
    }

    Component.onCompleted: {
        appTheme.mode = themePreference.mode
        windowChrome.applyAppearance(root, appTheme.dark)
        Windows.registerWindow(root)
        root.applyNewDocumentPreferences(root.documents.documentAt(0))
        if (root.adoptDocument) {
            root.documents.adopt(root.adoptDocument)
            root.documents.close(0)
        }
    }
    Connections {
        target: root.documents
        function onDocumentAdded(document) { root.applyNewDocumentPreferences(document) }
    }
    Connections {
        target: root.themePreference
        function onModeChanged() { appTheme.mode = root.themePreference.mode }
    }
    // The controller owns part colours for the session; the preference remembers them.
    Connections {
        target: root.controller
        function onRenderModeChanged() { root.themePreference.renderMode = root.controller.renderMode }
        // Part colours are an appearance setting, so a change in one tab applies to all.
        function onPartColorsChanged() {
            root.themePreference.partColorMode = root.controller.partColorMode
            root.themePreference.recolorFileColors = root.controller.recolorFileColors
            root.themePreference.partColorSeed = root.controller.partColorSeed
            for (let index = 0; index < root.documents.count; ++index) {
                const document = root.documents.documentAt(index)
                if (document !== root.controller) root.applyPartColorPreferences(document)
            }
        }
    }
    readonly property var partColorModes: [
        { value: "file", label: qsTr("File Colours") },
        { value: "shades", label: qsTr("Auto: Cool Shades") },
        { value: "warm", label: qsTr("Auto: Warm Shades") },
        { value: "muted", label: qsTr("Auto: Muted") },
        { value: "vivid", label: qsTr("Auto: High Contrast") }
    ]

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
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: root.controller.hiddenPartCount > 0
                      ? qsTr("Export Assembly Without %n Hidden Part(s)…", "", root.controller.hiddenPartCount)
                      : qsTr("Export Assembly Without Hidden Parts…")
                enabled: root.controller.documentState === AppState.TreeReady && !root.controller.importInProgress
                         && !root.controller.assemblyExportInProgress
                onTriggered: root.chooseAssemblyExport()
            }
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("Export Exploded Assembly…")
                enabled: root.controller.documentState === AppState.TreeReady && !root.controller.importInProgress
                         && !root.controller.assemblyExportInProgress && root.controller.explode.exploded
                onTriggered: root.controller.requestExplodedExport()
            }
            Inspect.ThemedMenuSeparator { theme: appTheme }
            // Windows only: Loupe ships without an installer, so this is how it becomes an app
            // that can open STEP files. Windows then asks the user to confirm the default.
            Inspect.ThemedMenuItem {
                theme: appTheme
                visible: root.fileAssociation.supported
                height: visible ? implicitHeight : 0
                text: root.fileAssociation.registered ? qsTr("Make Loupe the Default for STEP Files…")
                                                      : qsTr("Register Loupe for STEP Files…")
                onTriggered: {
                    if (!root.fileAssociation.registered) root.fileAssociation.registerForStepFiles()
                    root.fileAssociation.openDefaultAppsSettings()
                }
            }
            Inspect.ThemedMenuItem {
                theme: appTheme
                action: Action {
                    text: qsTr("Close Tab")
                    shortcut: StandardKey.Close
                    onTriggered: root.requestCloseTab(root.documents.activeIndex)
                }
            }
            Inspect.ThemedMenuItem {
                theme: appTheme
                action: Action {
                    text: qsTr("Next Tab")
                    shortcut: StandardKey.NextChild
                    enabled: root.documents.count > 1
                    onTriggered: root.documents.activeIndex = (root.documents.activeIndex + 1) % root.documents.count
                }
            }
            Inspect.ThemedMenuItem {
                theme: appTheme
                action: Action {
                    text: qsTr("Previous Tab")
                    shortcut: StandardKey.PreviousChild
                    enabled: root.documents.count > 1
                    onTriggered: root.documents.activeIndex = (root.documents.activeIndex + root.documents.count - 1) % root.documents.count
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
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("Drawing")
                checkable: true
                checked: root.controller.workspace === AppState.Drawing
                onTriggered: root.controller.setWorkspace(AppState.Drawing)
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
            Inspect.ThemedMenuItem {
                theme: appTheme
                // Ctrl+8 (Cmd+8 on macOS): the binding SOLIDWORKS users know for Normal To.
                action: Action {
                    text: qsTr("Normal to Face")
                    shortcut: "Ctrl+8"
                    enabled: root.controller.documentState !== AppState.Empty
                    onTriggered: root.controller.viewNormalToFace()
                }
            }
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: root.controller.visibilityRevision >= 0 && root.controller.selectedNodeCount >= 0
                      && root.controller.isSelectionTransparent() ? qsTr("Make Selection Opaque") : qsTr("Make Selection Transparent")
                enabled: root.controller.activeNodeId.length > 0
                onTriggered: root.controller.toggleTransparentSelection()
            }
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("Make Others Transparent")
                enabled: root.controller.activeNodeId.length > 0
                onTriggered: root.controller.makeOthersTransparent()
            }
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("Show All")
                onTriggered: root.controller.showAllNodes()
            }
            Inspect.ThemedMenuSeparator { theme: appTheme }
            Inspect.ThemedMenu {
                theme: appTheme
                title: qsTr("Part Colours")
                Repeater {
                    model: root.partColorModes
                    delegate: Inspect.ThemedMenuItem {
                        required property var modelData
                        theme: appTheme
                        text: modelData.label
                        checkable: true
                        checked: root.controller.partColorMode === modelData.value
                        onTriggered: root.controller.partColorMode = modelData.value
                    }
                }
                Inspect.ThemedMenuSeparator { theme: appTheme }
                Inspect.ThemedMenuItem {
                    theme: appTheme
                    text: qsTr("Shuffle Colours")
                    enabled: root.controller.partColorMode !== "file"
                    onTriggered: root.controller.shufflePartColors()
                }
                Inspect.ThemedMenuItem {
                    theme: appTheme
                    text: qsTr("Recolour Parts With File Colours")
                    checkable: true
                    enabled: root.controller.partColorMode !== "file"
                    checked: root.controller.recolorFileColors
                    onTriggered: root.controller.recolorFileColors = checked
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
                text: qsTr("Check for Updates…")
                enabled: Updates.supported && Updates.state !== Updates.Checking
                         && Updates.state !== Updates.Downloading && Updates.state !== Updates.Installing
                onTriggered: Updates.checkForUpdates(true)
            }
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("Include Beta Versions")
                checkable: true
                checked: Updates.includePrereleases
                enabled: Updates.supported
                onTriggered: Updates.includePrereleases = checked
            }
            Inspect.ThemedMenuSeparator { theme: appTheme }
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("What's New in Loupe…")
                enabled: Updates.whatsNewItems.length > 0
                onTriggered: whatsNew.open()
            }
            Inspect.ThemedMenuItem {
                theme: appTheme
                text: qsTr("About Loupe")
                onTriggered: aboutLoupe.open()
            }
        }
    }

    header: ColumnLayout {
        spacing: 0
        ToolBar {
        Layout.fillWidth: true
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
                model: root.workspaces.map(function(entry) { return entry.label })
                currentIndex: root.workspaceIndex
                implicitWidth: 252
                Accessible.name: qsTr("Workspace")
                onActivated: index => root.controller.setWorkspace(root.workspaces[index].state)
            }
            Item { Layout.fillWidth: true }
            // A newer Loupe (#3): a download icon whose ring fills as it downloads.
            Inspect.UpdateIndicator {
                objectName: "updateIndicator"
                theme: appTheme
            }
            Inspect.ThemedButton {
                text: qsTr("Units: %1").arg(root.controller.effectiveUnit)
                theme: appTheme
                Accessible.name: qsTr("Review source units")
                onClicked: unitReview.open()
            }
            // System, light or dark: three positions, one click each, rather than a menu.
            Inspect.ThemedSegmentedControl {
                id: themeChooser
                objectName: "themeChooser"
                theme: appTheme
                model: [qsTr("Follow the system"), qsTr("Light"), qsTr("Dark")]
                icons: ["themeSystem", "themeLight", "themeDark"]
                currentIndex: root.themePreference.mode === "Light" ? 1 : root.themePreference.mode === "Dark" ? 2 : 0
                Accessible.name: qsTr("Color theme")
                onActivated: function(index) {
                    const mode = index === 1 ? "Light" : index === 2 ? "Dark" : "System"
                    appTheme.mode = mode
                    root.themePreference.mode = mode
                }
            }
        }
    }

        // Document tabs: shown once a file is open, or when there is more than one tab.
        Rectangle {
            id: tabStrip
            objectName: "documentTabStrip"
            Layout.fillWidth: true
            visible: root.documents.count > 1 || root.controller.documentName.length > 0 || tabDropIndicator.shown
            implicitHeight: visible ? appTheme.rowCompact + appTheme.spacing2 * 2 : 0
            // Where a tab dragged from another window would land: a pulsing accent bar
            // between tabs, the way a text caret marks an insertion point.
            Rectangle {
                id: tabDropIndicator
                objectName: "tabDropIndicator"
                property int index: 0
                // Its own flag: `visible` reads false while the strip is still hidden, which
                // would keep the strip hidden for good.
                property bool shown: false
                readonly property Item before: shown && index < root.documents.count ? tabRepeater.itemAt(index) : null
                readonly property Item last: shown && root.documents.count > 0 ? tabRepeater.itemAt(root.documents.count - 1) : null
                readonly property real rowX: before ? before.x - tabRow.spacing / 2
                                           : last ? last.x + last.width + tabRow.spacing / 2 : 0
                visible: shown
                z: 5
                x: tabRow.x + rowX - width / 2
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: appTheme.rowCompact
                radius: 1.5
                color: appTheme.accent
                SequentialAnimation on opacity {
                    running: tabDropIndicator.shown
                    loops: Animation.Infinite
                    NumberAnimation { from: 1; to: 0.25; duration: appTheme.durIndeterminatePulse; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 0.25; to: 1; duration: appTheme.durIndeterminatePulse; easing.type: Easing.InOutSine }
                }
            }
            color: appTheme.surface
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: appTheme.border
            }
            RowLayout {
                id: tabRow
                anchors.fill: parent
                anchors.leftMargin: appTheme.spacing3
                anchors.rightMargin: appTheme.spacing3
                spacing: appTheme.spacing1
                Repeater {
                    id: tabRepeater
                    model: root.documents
                    delegate: Rectangle {
                        id: tab
                        objectName: "documentTab"
                        required property int index
                        required property string name
                        required property QtObject document
                        readonly property bool active: index === root.documents.activeIndex
                        Layout.preferredHeight: appTheme.rowCompact
                        Layout.maximumWidth: 240
                        Layout.preferredWidth: tabLabel.implicitWidth + closeTab.width + appTheme.spacing3 * 2
                        opacity: tabDrag.active && tabDrag.outside ? 0.45 : 1
                        radius: appTheme.radius2
                        color: active ? appTheme.surfaceRaised : tabHover.hovered ? appTheme.surfaceSubtle : "transparent"
                        border.color: active ? appTheme.border : "transparent"
                        HoverHandler { id: tabHover }
                        // Drag along the strip to reorder; drag off it and let go to move the
                        // document into a new window of its own, as Chrome does (#15).
                        DragHandler {
                            id: tabDrag
                            target: null
                            dragThreshold: 6
                            property bool outside: false
                            // The window under the pointer, if another one: it shows where the
                            // tab would land, since this window cannot draw over it.
                            property var markedTarget: null
                            function markTarget(target, x, y) {
                                if (markedTarget && markedTarget !== target) markedTarget.hideTabDropIndicator()
                                markedTarget = target
                                if (target) target.showTabDropIndicator(x, y)
                            }
                            onCentroidChanged: {
                                if (!active) return
                                const scene = centroid.scenePosition
                                const inStrip = tabRow.mapFromItem(null, scene.x, scene.y)
                                // Over another window's tab strip, the tab goes there -- even
                                // a window's only tab, which then closes its window.
                                const dropTarget = Windows.dropTargetAt(root.x + scene.x, root.y + scene.y, root)
                                markTarget(dropTarget, root.x + scene.x, root.y + scene.y)
                                outside = dropTarget !== null
                                          || (root.documents.count > 1
                                              && (inStrip.y < -tabStrip.height || inStrip.y > tabStrip.height * 2.5))
                                tabGhost.x = scene.x - tabGhost.width / 2
                                tabGhost.y = scene.y - tabGhost.height / 2
                                tabGhost.text = tabLabel.text
                                tabGhost.hint = dropTarget ? qsTr("Release to move into that window")
                                                           : qsTr("Release to open in a new window")
                                tabGhost.visible = outside
                                if (outside) return
                                // The slot before the first other tab whose centre is right of
                                // the pointer, as a final index: so a tab swaps with a neighbour
                                // once dragged past that neighbour's centre.
                                let before = root.documents.count
                                for (let i = 0; i < root.documents.count; ++i) {
                                    const other = tabRepeater.itemAt(i)
                                    if (i !== tab.index && other && inStrip.x < other.x + other.width / 2) { before = i; break }
                                }
                                const target = before > tab.index ? before - 1 : before
                                if (target !== tab.index) root.documents.move(tab.index, target)
                            }
                            onActiveChanged: {
                                if (active) {
                                    root.documents.activeIndex = tab.index
                                    return
                                }
                                tabGhost.visible = false
                                markTarget(null, 0, 0)
                                if (!outside) return
                                outside = false
                                // Everything needed is read first: take() removes this tab's row,
                                // which destroys this delegate and its access to root.
                                const scene = centroid.scenePosition
                                const window = root
                                const documents = root.documents
                                const index = tab.index
                                const x = root.x + scene.x
                                const y = root.y + scene.y
                                const dropTarget = Windows.dropTargetAt(x, y, window)
                                if (dropTarget) {
                                    const dropIndex = dropTarget.tabDropIndexAt(x, y)
                                    // A window's last tab can leave too: an empty one stands in
                                    // until this window closes behind it, as in Chrome.
                                    const emptied = documents.count === 1
                                    if (emptied) documents.add()
                                    const moving = documents.take(index)
                                    if (moving) dropTarget.adoptDocumentAt(moving, dropIndex)
                                    if (emptied) window.close()
                                    return
                                }
                                const document = documents.take(index)
                                // If no window could be made, the tab comes straight back.
                                if (document && !Windows.openWindowWith(document, window, x, y))
                                    documents.adopt(document)
                            }
                        }
                        TapHandler {
                            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                            onTapped: function(eventPoint, button) {
                                if (button === Qt.MiddleButton) root.requestCloseTab(tab.index)
                                else root.documents.activeIndex = tab.index
                            }
                        }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: appTheme.spacing3
                            anchors.rightMargin: appTheme.spacing1
                            spacing: appTheme.spacing1
                            Label {
                                id: tabLabel
                                Layout.fillWidth: true
                                text: tab.name.length > 0 ? tab.name : qsTr("New tab")
                                color: tab.active ? appTheme.foreground : appTheme.muted
                                font.weight: tab.active ? Font.DemiBold : Font.Normal
                                elide: Text.ElideMiddle
                            }
                            Inspect.IconButton {
                                id: closeTab
                                theme: appTheme
                                // Sized inside the 28 px tab, so its hover highlight stays in it (#16).
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: 20
                                implicitHeight: 20
                                iconSize: 14
                                glyph: "close"
                                Accessible.name: qsTr("Close %1").arg(tabLabel.text)
                                onClicked: root.requestCloseTab(tab.index)
                            }
                        }
                        Inspect.ThemedToolTip {
                            theme: appTheme
                            text: tab.document ? tab.document.documentPath : ""
                            visible: tabHover.hovered && text.length > 0
                        }
                    }
                }
                Inspect.ThemedToolButton {
                    theme: appTheme
                    text: "+"
                    Accessible.name: qsTr("Open a STEP file in a new tab")
                    onClicked: {
                        root.documents.add()
                        openStepDialog.open()
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // A full set of workspaces per tab, kept alive while hidden, so switching tabs keeps
    // each document's camera, selection, section and export queue.
    StackLayout {
        anchors.fill: parent
        currentIndex: root.documents.activeIndex

        Repeater {
            model: root.documents
            delegate: StackLayout {
                id: documentView
                required property QtObject document
                // Index order must match root.workspaces.
                currentIndex: root.workspaceIndexOf(documentView.document)

                Inspect.InspectWorkspace {
                    controller: documentView.document
                    theme: appTheme
                    preferences: root.themePreference
                    onOpenFileRequested: openStepDialog.open()
                }
                Export.ExportWorkspace {
                    controller: documentView.document
                    theme: appTheme
                    preferences: root.themePreference
                }
                Drawing.DrawingWorkspace {
                    objectName: "drawingWorkspace"
                    controller: documentView.document
                    theme: appTheme
                    preferences: root.themePreference
                }
            }
        }
    }

    // The tab being dragged out, following the pointer until it is let go.
    Rectangle {
        id: tabGhost
        property alias text: tabGhostLabel.text
        property alias hint: tabGhostHint.text
        parent: root.contentItem
        z: 400
        visible: false
        width: tabGhostLabel.implicitWidth + appTheme.spacing4 * 2
        height: appTheme.rowCompact + appTheme.spacing2
        radius: appTheme.radius2
        color: appTheme.surfaceRaised
        border.color: appTheme.accent
        Label {
            id: tabGhostLabel
            anchors.centerIn: parent
            color: appTheme.foreground
            font.weight: Font.DemiBold
        }
        Label {
            id: tabGhostHint
            anchors.top: parent.bottom
            anchors.topMargin: 4
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Release to open in a new window")
            color: appTheme.muted
            font.pixelSize: appTheme.fontCaption
        }
    }

    Popup {
        id: closeBusyTab
        property int tabIndex: -1
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(440, Overlay.overlay.width - appTheme.spacing6 * 2)
        padding: appTheme.spacing6
        background: Inspect.ElevatedPanel { theme: appTheme; cornerRadius: appTheme.radius4 }
        contentItem: ColumnLayout {
            spacing: appTheme.spacing3
            Label {
                text: qsTr("Close this tab?")
                font.bold: true
                font.pixelSize: 16
                color: root.themeForeground
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("An export or drawing run is still going in this tab. Closing it stops the run.")
                color: root.themeForeground
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Inspect.ThemedButton {
                    theme: appTheme
                    text: qsTr("Keep open")
                    onClicked: closeBusyTab.close()
                }
                Inspect.ThemedButton {
                    theme: appTheme
                    primary: true
                    text: qsTr("Stop and close")
                    onClicked: {
                        closeBusyTab.close()
                        root.documents.close(closeBusyTab.tabIndex)
                    }
                }
            }
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

    // The window's parts the sheets blur behind them.
    readonly property var sheetBackdropSources: [root.menuBar, root.header, root.contentItem].filter(function(item) { return item })

    Inspect.AboutLoupeDialog {
        id: aboutLoupe
        theme: appTheme
        version: root.controller.applicationVersion
        onWhatsNewRequested: whatsNew.open()
    }

    Inspect.InteractionGuide {
        id: interactionGuide
        theme: appTheme
    }

    // After an update, once per release: what is new in it.
    Inspect.WhatsNewDialog {
        id: whatsNew
        theme: appTheme
    }
    // Behind those two sheets: the window blurred and tinted, in the overlay under them.
    Inspect.ModalBackdrop {
        parent: Overlay.overlay
        theme: appTheme
        shown: aboutLoupe.visible || whatsNew.visible
        sources: root.sheetBackdropSources
    }
    // A moment after the window is up, so it opens over a drawn app rather than with it.
    Timer {
        interval: 700
        running: Updates.whatsNewPending && root.visible
        onTriggered: if (Updates.whatsNewPending && !whatsNew.opened) whatsNew.open()
    }

    FileDialog {
        id: assemblyExportDialog
        title: qsTr("Export assembly without hidden parts")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("STEP files (*.step *.stp)")]
        defaultSuffix: "step"
        onAccepted: root.controller.exportAssemblyWithoutHiddenParts(selectedFile)
    }

    // A whole-assembly export: its progress while it runs, then how it went. A neutral card
    // at the bottom right (design.md toasts), not over the dock; a success clears itself,
    // a failure stays until it is read and closed.
    Rectangle {
        id: assemblyExportNotice
        objectName: "assemblyExportNotice"
        readonly property bool running: root.controller.assemblyExportInProgress
        readonly property bool failed: !running && !root.controller.assemblyExportSucceeded
        z: 300
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: appTheme.spacing4
        anchors.bottomMargin: appTheme.spacing4
        width: Math.min(parent.width - appTheme.spacing4 * 2, 360)
        height: noticeContent.implicitHeight + appTheme.spacing3 * 2
        radius: appTheme.radius3
        color: appTheme.surface3
        border.color: appTheme.border
        visible: false

        RowLayout {
            id: noticeContent
            anchors.fill: parent
            anchors.margins: appTheme.spacing3
            spacing: appTheme.spacing3

            Inspect.ToolIcon {
                Layout.alignment: Qt.AlignTop
                name: assemblyExportNotice.running ? "download" : assemblyExportNotice.failed ? "alert" : "check"
                color: assemblyExportNotice.running ? appTheme.accent
                     : assemblyExportNotice.failed ? appTheme.error : appTheme.success
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: appTheme.spacing1
                Label {
                    objectName: "assemblyExportNoticeTitle"
                    Layout.fillWidth: true
                    text: assemblyExportNotice.running ? qsTr("Exporting the assembly")
                        : assemblyExportNotice.failed ? qsTr("Export failed") : qsTr("Assembly exported")
                    font.bold: true
                    color: root.themeForeground
                    elide: Text.ElideRight
                }
                Label {
                    objectName: "assemblyExportNoticeDetail"
                    Layout.fillWidth: true
                    text: assemblyExportNotice.running
                          ? qsTr("%1 · %2%").arg(root.controller.assemblyExportStage)
                                            .arg(Math.round(root.controller.assemblyExportProgress * 100))
                          : root.controller.assemblyExportMessage
                    color: appTheme.muted
                    font.pixelSize: appTheme.fontCaption
                    font.features: { "tnum": 1 }
                    wrapMode: Text.Wrap
                }
                Inspect.ThemedProgressBar {
                    theme: appTheme
                    Layout.fillWidth: true
                    Layout.topMargin: appTheme.spacing1
                    visible: assemblyExportNotice.running
                    from: 0
                    to: 1
                    value: root.controller.assemblyExportProgress
                }
            }
            Inspect.IconButton {
                Layout.alignment: Qt.AlignTop
                theme: appTheme
                visible: !assemblyExportNotice.running
                glyph: "close"
                Accessible.name: qsTr("Dismiss")
                onClicked: assemblyExportNotice.visible = false
            }
        }
        Timer { id: noticeTimer; interval: 5000; onTriggered: assemblyExportNotice.visible = false }
        Connections {
            target: root.controller
            function onAssemblyExportChanged() {
                assemblyExportNotice.visible = root.controller.assemblyExportInProgress
                                               || root.controller.assemblyExportMessage.length > 0
                noticeTimer.stop()
                if (!root.controller.assemblyExportInProgress && root.controller.assemblyExportSucceeded)
                    noticeTimer.restart()
            }
        }
    }

    OpenStepDialog {
        id: openStepDialog
        onStepFileChosen: function(file) { root.openStepFile(file) }
    }
}
