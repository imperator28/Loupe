import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A list of saved snapshots (SavedViews entries: id, name, summary, thumbnail), newest first:
// a few rows, then it scrolls. Clicking a row goes to it; double-clicking, or the pencil on
// hover, renames it; the cross deletes it; where `canSaveOver`, a circling arrow saves what is
// on screen now over it. The one on screen is marked with the accent leading bar the tree
// uses for its selection. Shared by the Views panel and the exploded view's saved layouts.
ListView {
    id: list

    property QtObject theme
    // The SavedViews store the entries come from: renames and deletes go to it.
    property QtObject store: null
    property var entries: []
    property string activeId: ""
    // The entry whose name is being typed.
    property string editingId: ""
    property string placeholderIcon: "views"
    property string emptyText: ""
    property int visibleRows: 5
    property bool canSaveOver: false
    property bool saveOverEnabled: true
    signal activated(string id)
    signal saveOverRequested(string id)

    readonly property int rowHeight: 60
    implicitHeight: count === 0 ? emptyState.implicitHeight + 24
                                : Math.min(count, visibleRows) * rowHeight + Math.max(0, Math.min(count, visibleRows) - 1) * spacing
    clip: true
    spacing: 4
    model: entries
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: list.contentHeight > list.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

    Label {
        id: emptyState
        visible: list.count === 0
        anchors.centerIn: parent
        width: list.width - 24
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: list.theme.muted
        font.pixelSize: list.theme.fontCaption
        text: list.emptyText
    }

    delegate: Rectangle {
        id: row
        required property var modelData
        required property int index
        readonly property bool active: modelData.id === list.activeId
        readonly property bool editing: modelData.id === list.editingId
        objectName: "viewsRow-" + index
        width: ListView.view.width
        height: list.rowHeight
        radius: list.theme.radius2
        color: active ? list.theme.accentTint
             : rowHover.hovered ? Qt.rgba(list.theme.accentTint.r, list.theme.accentTint.g,
                                          list.theme.accentTint.b, list.theme.accentTint.a * 0.55)
             : "transparent"
        Behavior on color { ColorAnimation { duration: list.theme.durInstant } }

        // Current: the accent leading bar the tree uses for its selection.
        Rectangle {
            visible: row.active
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 2
            height: parent.height - 16
            radius: 1
            color: list.theme.accent
        }

        HoverHandler { id: rowHover }
        TapHandler {
            onTapped: if (!row.editing) list.activated(row.modelData.id)
            onDoubleTapped: list.editingId = row.modelData.id
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
                radius: list.theme.radius1
                color: list.theme.viewport
                border.color: list.theme.border
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
                    name: list.placeholderIcon
                    color: list.theme.muted
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
                    color: list.theme.foreground
                    font.bold: row.active
                }
                ThemedTextField {
                    id: nameField
                    objectName: "viewsNameField"
                    theme: list.theme
                    Layout.fillWidth: true
                    visible: row.editing
                    text: row.modelData.name
                    onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                    Component.onCompleted: if (visible) { forceActiveFocus(); selectAll() }
                    Keys.onEscapePressed: list.editingId = ""
                    onAccepted: {
                        list.store.rename(row.modelData.id, text)
                        list.editingId = ""
                    }
                    onActiveFocusChanged: if (!activeFocus && row.editing) {
                        list.store.rename(row.modelData.id, text)
                        list.editingId = ""
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: !row.editing
                    text: row.modelData.summary
                    elide: Text.ElideRight
                    color: list.theme.muted
                    font.pixelSize: list.theme.fontCaption
                }
            }

            // Save over, rename and delete, on hover (and for the row being renamed, nothing).
            IconButton {
                objectName: "viewsSaveOver-" + row.index
                theme: list.theme
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 26
                implicitHeight: 26
                cornerRadius: list.theme.radius1
                // On hover only, so at rest the name and summary have the row's width.
                visible: list.canSaveOver && !row.editing && rowHover.hovered
                enabled: list.saveOverEnabled
                glyph: "saveOver"
                Accessible.name: qsTr("Save the current layout over %1").arg(row.modelData.name)
                onClicked: list.saveOverRequested(row.modelData.id)
                ThemedToolTip { theme: list.theme; text: qsTr("Save what is on screen over this"); visible: parent.hovered }
            }
            IconButton {
                theme: list.theme
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 26
                implicitHeight: 26
                cornerRadius: list.theme.radius1
                visible: !row.editing && rowHover.hovered
                glyph: "rename"
                Accessible.name: qsTr("Rename %1").arg(row.modelData.name)
                onClicked: list.editingId = row.modelData.id
                ThemedToolTip { theme: list.theme; text: qsTr("Rename"); visible: parent.hovered }
            }
            IconButton {
                objectName: "viewsDelete-" + row.index
                theme: list.theme
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 26
                implicitHeight: 26
                cornerRadius: list.theme.radius1
                visible: !row.editing && rowHover.hovered
                glyph: "close"
                Accessible.name: qsTr("Delete %1").arg(row.modelData.name)
                onClicked: list.store.remove(row.modelData.id)
                ThemedToolTip { theme: list.theme; text: qsTr("Delete"); visible: parent.hovered }
            }
        }
    }
}
