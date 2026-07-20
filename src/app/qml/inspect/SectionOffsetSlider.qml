import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root

    property QtObject theme
    property real value: 0
    property real from: -1
    property real to: 1
    property real resetValue: 0
    signal valueEdited(real value)
    signal interactionStarted()
    signal interactionCommitted()
    signal interactionCanceled()

    width: 64
    height: 208
    focus: true
    Accessible.name: qsTr("Section offset")
    Accessible.role: Accessible.Slider
    Accessible.description: Number(value).toLocaleString(Qt.locale(), "f", 2)

    function boundedValue(candidate) {
        return Math.max(from, Math.min(to, candidate))
    }

    function valueAt(y) {
        const ratio = Math.max(0, Math.min(1, (track.bottom - y) / track.height))
        return from + ratio * (to - from)
    }

    function valueForDrag(initialValue, deltaY, fine) {
        const sensitivity = fine ? 0.1 : 1
        const candidate = initialValue - deltaY * (to - from) / track.height * sensitivity
        return boundedValue(candidate)
    }

    function moveBy(direction, fine) {
        const increment = Math.max((to - from) / (fine ? 500 : 100), 0.0001)
        valueEdited(boundedValue(value + direction * increment))
    }

    Keys.onUpPressed: function(event) {
        moveBy(1, event.modifiers & Qt.ShiftModifier)
        event.accepted = true
    }
    Keys.onDownPressed: function(event) {
        moveBy(-1, event.modifiers & Qt.ShiftModifier)
        event.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: root.theme.surfaceRaised
        border.color: root.theme.border
    }

    Rectangle {
        id: track
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 16
        anchors.bottomMargin: 16
        width: 4
        radius: 2
        color: root.theme.border

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(0, Math.min(parent.height - height,
                    parent.height - ((root.value - root.from) / Math.max(0.000001, root.to - root.from)) * parent.height - height / 2))
            width: 30
            height: 18
            radius: 3
            color: root.theme.accent
            border.color: root.theme.surfaceRaised
            border.width: 2
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        preventStealing: true
        property real startY: 0
        property real startValue: 0

        onPressed: function(mouse) {
            root.forceActiveFocus()
            root.interactionStarted()
            startY = mouse.y
            startValue = root.value
        }
        onPositionChanged: function(mouse) {
            if (!(pressedButtons & Qt.LeftButton)) return
            root.valueEdited(root.valueForDrag(startValue, mouse.y - startY,
                                               mouse.modifiers & Qt.ShiftModifier))
        }
        onReleased: root.interactionCommitted()
        onCanceled: root.interactionCanceled()
        onDoubleClicked: root.valueEdited(root.boundedValue(root.resetValue))
    }
}
