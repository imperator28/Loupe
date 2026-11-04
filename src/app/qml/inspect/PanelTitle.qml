import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The heading of a side panel, the same in every workspace: the title in bold at the
// title size, an optional muted detail (a count) at the right, and room for small icon
// buttons after it. Always one 28 px row, with or without buttons, so the content below
// starts at the same height in every panel.
RowLayout {
    id: root

    property QtObject theme
    property string text
    property string detail
    default property alias trailing: trailingRow.data

    Layout.fillWidth: true
    Layout.preferredHeight: 28
    spacing: theme ? theme.spacing1 : 4

    Label {
        objectName: "panelTitleText"
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        text: root.text
        color: root.theme ? root.theme.foreground : "transparent"
        font.bold: true
        font.pixelSize: root.theme ? root.theme.fontTitle : 15
        elide: Text.ElideRight
    }
    Label {
        Layout.alignment: Qt.AlignVCenter
        visible: root.detail.length > 0
        text: root.detail
        color: root.theme ? root.theme.muted : "transparent"
        font.features: { "tnum": 1 }
    }
    RowLayout {
        id: trailingRow
        Layout.alignment: Qt.AlignVCenter
        spacing: root.theme ? root.theme.spacing1 : 4
        visible: children.length > 0
    }
}
