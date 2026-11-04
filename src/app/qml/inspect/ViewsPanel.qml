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
    // 0 Display, 1 Camera.
    property int kindIndex: 0
    readonly property bool camera: kindIndex === 1
    readonly property var entries: !store ? [] : camera ? store.cameraViews : store.displayStates
    readonly property string activeId: !viewport ? ""
        : camera ? viewport.activeCameraViewId : viewport.activeDisplayStateId
    // The entry whose name is being typed.
    property string editingId: ""

    function saveCurrent() {
        if (!viewport) return
        const id = camera ? viewport.saveCameraView("") : viewport.saveDisplayState("")
        if (id.length > 0) editingId = id
    }

    function goTo(id) {
        if (!viewport) return
        editingId = ""
        if (camera) viewport.goToCameraView(id)
        else viewport.goToDisplayState(id)
    }

    function resetToDefault() {
        if (camera) { if (viewport) viewport.fitCamera() }
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
            model: [qsTr("Display"), qsTr("Camera")]
            currentIndex: root.kindIndex
            Accessible.name: qsTr("Saved display states or camera views")
            onActivated: function(index) { root.editingId = ""; root.kindIndex = index }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.theme.muted
            font.pixelSize: root.theme.fontCaption
            text: root.camera ? qsTr("Where the camera is: angle, zoom and projection. Parts shown stay as they are.")
                              : qsTr("What is hidden or transparent. The camera stays where it is.")
        }

        ThemedButton {
            objectName: "viewsSaveCurrent"
            theme: root.theme
            Layout.fillWidth: true
            enabled: root.viewport !== null && root.documentController !== null
            text: root.camera ? qsTr("Save Current View") : qsTr("Save Current Display")
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
        ListView {
            id: list
            objectName: "viewsList"
            Layout.fillWidth: true
            Layout.preferredHeight: count === 0 ? emptyState.implicitHeight + 24
                                                : Math.min(count, 5) * 60 + Math.max(0, Math.min(count, 5) - 1) * spacing
            visible: true
            clip: true
            spacing: 4
            model: root.entries
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: list.contentHeight > list.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

            Label {
                id: emptyState
                visible: list.count === 0
                anchors.centerIn: parent
                width: list.width - 24
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: root.theme.muted
                font.pixelSize: root.theme.fontCaption
                text: root.camera ? qsTr("No saved views yet. Turn and zoom to a view you want again, then save it.")
                                  : qsTr("No saved displays yet. Hide parts or make them transparent, then save it to come back to.")
            }

            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                readonly property bool active: modelData.id === root.activeId
                readonly property bool editing: modelData.id === root.editingId
                objectName: "viewsRow-" + index
                width: ListView.view.width
                height: 60
                radius: root.theme.radius2
                color: active ? root.theme.accentTint
                     : rowHover.hovered ? Qt.rgba(root.theme.accentTint.r, root.theme.accentTint.g,
                                                  root.theme.accentTint.b, root.theme.accentTint.a * 0.55)
                     : "transparent"
                Behavior on color { ColorAnimation { duration: root.theme.durInstant } }

                // Current: the accent leading bar the tree uses for its selection.
                Rectangle {
                    visible: row.active
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 2
                    height: parent.height - 16
                    radius: 1
                    color: root.theme.accent
                }

                HoverHandler { id: rowHover }
                TapHandler {
                    onTapped: if (!row.editing) root.goTo(row.modelData.id)
                    onDoubleTapped: root.editingId = row.modelData.id
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 6
                    spacing: 10

                    // The thumbnail, cropped to a 16:10 card.
                    Rectangle {
                        Layout.preferredWidth: 72
                        Layout.preferredHeight: 45
                        Layout.alignment: Qt.AlignVCenter
                        radius: root.theme.radius1
                        color: root.theme.viewport
                        border.color: root.theme.border
                        clip: true
                        Image {
                            anchors.fill: parent
                            anchors.margins: 1
                            source: row.modelData.thumbnail
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                            smooth: true
                            mipmap: true
                            visible: status === Image.Ready
                        }
                        ToolIcon {
                            anchors.centerIn: parent
                            width: 18
                            height: 18
                            visible: !row.modelData.thumbnail || String(row.modelData.thumbnail).length === 0
                            name: root.camera ? "viewCube" : "views"
                            color: root.theme.muted
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            visible: !row.editing
                            text: row.modelData.name
                            elide: Text.ElideRight
                            color: root.theme.foreground
                            font.bold: row.active
                        }
                        ThemedTextField {
                            id: nameField
                            objectName: "viewsNameField"
                            theme: root.theme
                            Layout.fillWidth: true
                            visible: row.editing
                            text: row.modelData.name
                            onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                            Component.onCompleted: if (visible) { forceActiveFocus(); selectAll() }
                            Keys.onEscapePressed: root.editingId = ""
                            onAccepted: {
                                root.store.rename(row.modelData.id, text)
                                root.editingId = ""
                            }
                            onActiveFocusChanged: if (!activeFocus && row.editing) {
                                root.store.rename(row.modelData.id, text)
                                root.editingId = ""
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: !row.editing
                            text: row.modelData.summary
                            elide: Text.ElideRight
                            color: root.theme.muted
                            font.pixelSize: root.theme.fontCaption
                        }
                    }

                    // Rename and delete, on hover (and for the row being renamed, nothing).
                    IconButton {
                        theme: root.theme
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: 26
                        implicitHeight: 26
                        cornerRadius: root.theme.radius1
                        visible: !row.editing
                        opacity: rowHover.hovered ? 1 : 0
                        enabled: opacity > 0
                        glyph: "rename"
                        Accessible.name: qsTr("Rename %1").arg(row.modelData.name)
                        onClicked: root.editingId = row.modelData.id
                        ThemedToolTip { theme: root.theme; text: qsTr("Rename"); visible: parent.hovered }
                    }
                    IconButton {
                        objectName: "viewsDelete-" + row.index
                        theme: root.theme
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: 26
                        implicitHeight: 26
                        cornerRadius: root.theme.radius1
                        visible: !row.editing
                        opacity: rowHover.hovered ? 1 : 0
                        enabled: opacity > 0
                        glyph: "close"
                        Accessible.name: qsTr("Delete %1").arg(row.modelData.name)
                        onClicked: root.store.remove(row.modelData.id)
                        ThemedToolTip { theme: root.theme; text: qsTr("Delete"); visible: parent.hovered }
                    }
                }
            }
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
                text: root.camera ? qsTr("Reset fits the whole model.") : qsTr("Reset shows every part, opaque.")
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
