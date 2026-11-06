import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Views: snapshots of the model to come back to, as in Photoshop's snapshots or a CAD
// tool's saved orientations and assembly views. Two kinds, one tab each:
//   Display -- what is hidden or transparent (each part's opacity and edges, isolate);
//   Camera  -- where the camera is: angle, zoom and projection.
// Saving names the snapshot at once and opens the name for editing; clicking one goes back
// to it (the camera glides there); Reset returns to the file as it opened. Kept per file on
// this computer, never written into it.
Rectangle {
    id: root

    property QtObject theme
    property QtObject documentController
    property Item viewport: null
    property bool embedded: false
    signal closeRequested()

    readonly property QtObject store: documentController ? documentController.savedViews : null
    // 0 Display, 1 Camera, 2 Exploded layouts.
    property int kindIndex: 0
    readonly property bool camera: kindIndex === 1
    readonly property bool layouts: kindIndex === 2
    readonly property var entries: !store ? [] : camera ? store.cameraViews : layouts ? store.explodeLayouts : store.displayStates
    readonly property string activeId: !viewport ? ""
        : camera ? viewport.activeCameraViewId : layouts ? viewport.activeExplodeLayoutId : viewport.activeDisplayStateId
    // A layout is saved from what is on screen: there has to be one, and not the assembly.
    readonly property bool canSaveLayout: documentController !== null && documentController.explode.exploded
                                          && documentController.explode.blend > 0.5

    function saveCurrent() {
        if (!viewport) return
        const id = camera ? viewport.saveCameraView("") : layouts ? viewport.saveExplodeLayout("") : viewport.saveDisplayState("")
        if (id.length > 0) list.editingId = id
    }

    function goTo(id) {
        if (!viewport) return
        list.editingId = ""
        if (camera) viewport.goToCameraView(id)
        else if (layouts) viewport.goToExplodeLayout(id)
        else viewport.goToDisplayState(id)
    }

    function resetToDefault() {
        if (camera) { if (viewport) viewport.fitCamera() }
        else if (layouts) { if (viewport) viewport.showAssembled() }
        else if (documentController) documentController.resetDisplayState()
    }

    implicitWidth: 320
    implicitHeight: content.implicitHeight + 32
    height: implicitHeight
    radius: root.theme.radius4
    color: embedded ? "transparent" : root.theme.surfaceRaised
    border.color: embedded ? "transparent" : root.theme.border

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        ThemedSegmentedControl {
            objectName: "viewsKindSwitch"
            theme: root.theme
            Layout.fillWidth: true
            model: [qsTr("Display"), qsTr("Camera"), qsTr("Exploded")]
            currentIndex: root.kindIndex
            Accessible.name: qsTr("Saved display states, camera views or exploded layouts")
            onActivated: function(index) { list.editingId = ""; root.kindIndex = index }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.theme.muted
            font.pixelSize: root.theme.fontCaption
            text: root.camera ? qsTr("Where the camera is: angle, zoom and projection. Parts shown stay as they are.")
                : root.layouts ? qsTr("Where each part has been moved and turned to. The camera stays where it is.")
                               : qsTr("What is hidden or transparent. The camera stays where it is.")
        }

        ThemedButton {
            objectName: "viewsSaveCurrent"
            theme: root.theme
            Layout.fillWidth: true
            enabled: root.viewport !== null && root.documentController !== null && (!root.layouts || root.canSaveLayout)
            text: root.camera ? qsTr("Save Current View") : root.layouts ? qsTr("Save Current Layout") : qsTr("Save Current Display")
            leftPadding: 34
            ToolIcon {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                name: "add"
                color: root.theme.foreground
            }
            onClicked: root.saveCurrent()
        }

        // The snapshots, newest first; a few rows, then it scrolls.
        SavedViewList {
            id: list
            objectName: "viewsList"
            theme: root.theme
            store: root.store
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            entries: root.entries
            activeId: root.activeId
            placeholderIcon: root.camera ? "viewCube" : root.layouts ? "explode" : "views"
            emptyText: root.camera ? qsTr("No saved views yet. Turn and zoom to a view you want again, then save it.")
                     : root.layouts ? qsTr("No saved layouts yet. Explode the model or move parts, then save the layout to come back to it.")
                                    : qsTr("No saved displays yet. Hide parts or make them transparent, then save it to come back to.")
            canSaveOver: root.layouts
            saveOverEnabled: root.canSaveLayout
            onActivated: function(id) { root.goTo(id) }
            onSaveOverRequested: function(id) { if (root.viewport) root.viewport.updateExplodeLayout(id) }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.theme.border
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: root.theme.muted
                font.pixelSize: root.theme.fontCaption
                text: root.camera ? qsTr("Reset fits the whole model.")
                    : root.layouts ? qsTr("Reset shows the assembly; saved layouts are kept.")
                                   : qsTr("Reset shows every part, opaque.")
            }
            ThemedButton {
                objectName: "viewsReset"
                theme: root.theme
                text: qsTr("Reset")
                onClicked: root.resetToDefault()
            }
        }
    }
}
