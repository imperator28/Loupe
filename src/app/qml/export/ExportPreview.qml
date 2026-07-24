import QtQuick
import QtQuick.Controls
import "../inspect" as Inspect

Inspect.ElevatedPanel {
    id: root

    property QtObject controller
    property string title
    property string emptyText
    property string displayOnlyNodeId
    property string highlightNodeId
    property bool requireSelection: false
    property bool selectionEnabled: false
    property bool loadEnabled: true
    readonly property bool viewportReady: viewportLoader.status === Loader.Ready

    wellSurface: true

    Loader {
        id: viewportLoader
        anchors.fill: parent
        // Inset by the panel's own corner radius so the viewport's square
        // corners never poke past the rounded card boundary and paint over
        // the curve (a 1 px inset was not enough clearance for that).
        anchors.margins: root.theme && root.theme.radius3 !== undefined ? root.theme.radius3 : 8
        active: root.loadEnabled && root.controller
        asynchronous: true
        source: "../inspect/StepViewport.qml"
        onLoaded: {
            item.controller = root.controller
            item.theme = Qt.binding(function() { return root.theme })
            item.presentationOnly = true
            item.renderModeControlVisible = true
            item.selectionEnabled = Qt.binding(function() { return root.selectionEnabled })
            item.componentHoverEnabled = false
            item.contextActionsEnabled = true
            item.requireDisplayFilter = root.requireSelection
            item.displayOnlyNodeId = Qt.binding(function() { return root.displayOnlyNodeId })
            item.externalHighlightNodeId = Qt.binding(function() { return root.highlightNodeId })
        }
    }

    Label {
        id: previewTitle
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: root.theme ? root.theme.spacing3 : 12
        text: root.title
        color: root.theme ? root.theme.foreground : "transparent"
        font.bold: true
        z: 3
    }

    Label {
        anchors.centerIn: parent
        visible: root.displayOnlyNodeId.length === 0 && root.emptyText.length > 0
        text: root.emptyText
        color: root.theme ? root.theme.muted : "transparent"
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        width: Math.min(parent.width - 32, 260)
        z: 3
    }
}
