import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property QtObject controller
    property QtObject theme
    property string title
    property string emptyText
    property string displayOnlyNodeId
    property string highlightNodeId
    property bool requireSelection: false
    property bool selectionEnabled: false
    property bool loadEnabled: true
    readonly property bool viewportReady: viewportLoader.status === Loader.Ready

    color: theme ? theme.viewport : "#f7f9fa"
    border.color: theme ? theme.border : "#b9c5cb"
    radius: 6

    Loader {
        id: viewportLoader
        anchors.fill: parent
        anchors.margins: 1
        active: root.loadEnabled && root.controller
        asynchronous: true
        source: "../inspect/StepViewport.qml"
        onLoaded: {
            item.controller = root.controller
            item.theme = Qt.binding(function() { return root.theme })
            item.presentationOnly = true
            item.selectionEnabled = Qt.binding(function() { return root.selectionEnabled })
            item.componentHoverEnabled = false
            item.contextActionsEnabled = true
            item.requireDisplayFilter = root.requireSelection
            item.displayOnlyNodeId = Qt.binding(function() { return root.displayOnlyNodeId })
            item.externalHighlightNodeId = Qt.binding(function() { return root.highlightNodeId })
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 10
        color: root.theme ? root.theme.surfaceRaised : "#ffffff"
        border.color: root.theme ? root.theme.border : "#b9c5cb"
        radius: 4
        implicitWidth: previewTitle.implicitWidth + 16
        implicitHeight: previewTitle.implicitHeight + 10
        z: 3

        Label {
            id: previewTitle
            anchors.centerIn: parent
            text: root.title
            color: root.theme ? root.theme.onSurface : "#172127"
            font.bold: true
        }
    }

    Label {
        anchors.centerIn: parent
        visible: root.displayOnlyNodeId.length === 0 && root.emptyText.length > 0
        text: root.emptyText
        color: root.theme ? root.theme.muted : "#53636c"
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        width: Math.min(parent.width - 32, 260)
        z: 3
    }
}
