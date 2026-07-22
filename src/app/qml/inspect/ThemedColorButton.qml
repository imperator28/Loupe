import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ThemedButton {
    id: control

    property color selectedColor: theme ? theme.accent : "transparent"
    property string actionText: qsTr("Choose color")

    Accessible.name: qsTr("%1, %2").arg(actionText).arg(selectedColor.toString().toUpperCase())

    contentItem: RowLayout {
        spacing: control.theme ? control.theme.spacing2 : 8

        Rectangle {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            radius: control.theme ? control.theme.radius1 : 4
            color: control.selectedColor
            border.color: control.theme ? control.theme.border : "transparent"
            border.width: 1
        }

        Label {
            text: control.actionText
            color: control.enabled ? control.foreground : control.subduedForeground
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Label {
            text: control.selectedColor.toString().toUpperCase()
            color: control.subduedForeground
            font.pixelSize: control.theme ? control.theme.fontCaption : 11
        }
    }
}
