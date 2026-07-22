import QtQuick
import QtQuick.Controls.Basic

// macOS-style segmented control: a tonal well with a sliding raised thumb.
// The thumb is the only moving element; arrow keys move the selection.
Item {
    id: control

    property QtObject theme
    property var model: []
    property int currentIndex: 0
    signal activated(int index)

    readonly property color wellFill: theme ? theme.surfaceSubtle : "transparent"
    readonly property color thumbFill: theme && theme.dark && theme.surface3 !== undefined ? theme.surface3
                                                                                           : theme ? theme.surfaceRaised : "transparent"
    readonly property color outline: theme ? theme.border : "transparent"
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"
    readonly property color accentFill: theme ? theme.accent : "transparent"
    readonly property int cornerRadius: theme && theme.radius2 !== undefined ? theme.radius2 : 6
    readonly property int slideDuration: theme && theme.durStandard !== undefined ? theme.durStandard : 0
    readonly property int ringWidth: theme && theme.focusRingWidth !== undefined ? theme.focusRingWidth : 2
    readonly property int segmentCount: model ? model.length : 0
    readonly property real segmentWidth: segmentCount > 0 ? (width - 4) / segmentCount : 0

    implicitHeight: theme && theme.controlHeight !== undefined ? theme.controlHeight : 30
    implicitWidth: Math.max(48 * Math.max(1, segmentCount) + 4, segmentRow.implicitWidth + 4)

    activeFocusOnTab: true
    Accessible.role: Accessible.PageTabList

    // Controlled component: clicking or pressing arrows emits activated();
    // the owner updates currentIndex (directly or via its controller).
    Keys.onLeftPressed: if (currentIndex > 0) activated(currentIndex - 1)
    Keys.onRightPressed: if (currentIndex < segmentCount - 1) activated(currentIndex + 1)

    Rectangle {
        anchors.fill: parent
        radius: control.cornerRadius
        color: control.wellFill
        border.color: control.outline
        border.width: 1
    }

    Rectangle {
        id: thumb
        x: 2 + control.currentIndex * control.segmentWidth
        y: 2
        width: control.segmentWidth
        height: parent.height - 4
        radius: control.cornerRadius - 1
        color: control.thumbFill
        border.color: control.outline
        border.width: 1

        Behavior on x {
            NumberAnimation {
                duration: control.slideDuration
                easing.type: Easing.BezierSpline
                easing.bezierCurve: control.theme && control.theme.easeMove !== undefined
                                    ? control.theme.easeMove : [0.2, 0.0, 0.0, 1.0, 1.0, 1.0]
            }
        }
    }

    Row {
        id: segmentRow
        anchors.fill: parent
        anchors.margins: 2

        Repeater {
            model: control.model

            AbstractButton {
                required property var modelData
                required property int index
                width: control.segmentWidth
                height: segmentRow.height
                Accessible.role: Accessible.PageTab
                Accessible.name: text
                text: modelData
                onClicked: {
                    if (control.currentIndex !== index)
                        control.activated(index)
                }

                contentItem: Text {
                    text: parent.text
                    color: control.currentIndex === parent.index ? control.foreground : control.subduedForeground
                    font.weight: control.currentIndex === parent.index ? Font.DemiBold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Item {}
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -(control.ringWidth + 1)
        radius: control.cornerRadius + control.ringWidth + 1
        color: "transparent"
        border.color: control.accentFill
        border.width: control.ringWidth
        visible: control.activeFocus
    }
}
