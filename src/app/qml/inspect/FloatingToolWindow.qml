import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A tool's panel as a small window over the viewport: drag it by its title bar to wherever
// it is out of the way, minimise it to just the title bar, or close it. It opens at its
// usual corner; once moved it stays where it was put (within the workspace) until the
// session ends. Double-click the title bar to minimise or restore, as on macOS.
FocusScope {
    id: root

    property QtObject theme
    property string title: ""
    property bool shown: false
    property bool minimized: false
    // Where it opens before it is moved, from the workspace's bottom-right corner.
    property real defaultRightMargin: 314
    property real defaultBottomMargin: 96
    // The panel shown in it; its implicit size sizes the window.
    default property alias content: body.data
    readonly property Item panel: body.children.length > 0 ? body.children[0] : null
    signal closeRequested()

    // Moved by the user: where to, before keeping it inside the workspace.
    property bool placed: false
    property real placedX: 0
    property real placedY: 0
    readonly property real margin: 8
    // 36 px: the 28 px buttons sit 4 px clear of the bar's edges, concentric with its corner.
    readonly property real titleHeight: 36
    readonly property real panelWidth: panel ? Math.max(panel.implicitWidth, 240) : 240
    readonly property real panelHeight: panel ? panel.implicitHeight : 0
    readonly property real maximumBodyHeight: Math.max(80, (parent ? parent.height : 600) - titleHeight - margin * 2)

    function clamped(value, low, high) { return Math.max(low, Math.min(high, value)) }

    visible: shown
    width: panelWidth
    height: titleHeight + (minimized ? 0 : Math.min(panelHeight, maximumBodyHeight))
    x: parent ? clamped(placed ? placedX : parent.width - defaultRightMargin - width, margin, parent.width - width - margin) : 0
    y: parent ? clamped(placed ? placedY : parent.height - defaultBottomMargin - height, margin, parent.height - height - margin) : 0
    z: 20

    // Pin it where it is, so minimising keeps the title bar in place.
    function pinHere() {
        placedX = x
        placedY = y
        placed = true
    }

    function toggleMinimized() {
        pinHere()
        minimized = !minimized
    }

    // A tool that opens again comes back expanded, where it was left.
    onShownChanged: if (shown) minimized = false

    // Rises in as it appears, as the panel did before it could move.
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: root.theme ? root.theme.durStandard : 0 } }

    // A flat raised surface, as a popover: no gradient, which read as a stray shade on a
    // window minimised to its title bar.
    ElevatedPanel {
        anchors.fill: parent
        theme: root.theme
        flat: true
        cornerRadius: root.theme && root.theme.radius3 !== undefined ? root.theme.radius3 : 8
    }

    Item {
        id: titleBar
        objectName: "toolWindowTitleBar"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.titleHeight

        MouseArea {
            id: dragArea
            anchors.fill: parent
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            property point grab: Qt.point(0, 0)
            onPressed: function(mouse) {
                const inParent = mapToItem(root.parent, mouse.x, mouse.y)
                grab = Qt.point(inParent.x - root.x, inParent.y - root.y)
                root.pinHere()
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                const inParent = mapToItem(root.parent, mouse.x, mouse.y)
                root.placedX = inParent.x - grab.x
                root.placedY = inParent.y - grab.y
            }
            onDoubleClicked: root.toggleMinimized()
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 4
            spacing: 2

            // A grip: the title bar is the handle.
            Grid {
                columns: 2
                spacing: 2
                Layout.alignment: Qt.AlignVCenter
                Layout.rightMargin: 6
                Repeater {
                    model: 6
                    delegate: Rectangle {
                        width: 2
                        height: 2
                        radius: 1
                        color: root.theme ? root.theme.muted : "transparent"
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: root.title
                font.bold: true
                elide: Text.ElideRight
                color: root.theme ? root.theme.foreground : "transparent"
            }
            // Collapses to the title bar and back: a chevron saying which way it goes.
            IconButton {
                theme: root.theme
                objectName: "toolWindowMinimizeButton"
                Layout.alignment: Qt.AlignVCenter
                glyph: root.minimized ? "chevronDown" : "chevronUp"
                Accessible.name: root.minimized ? qsTr("Restore %1").arg(root.title) : qsTr("Minimise %1").arg(root.title)
                onClicked: root.toggleMinimized()
                ThemedToolTip {
                    theme: root.theme
                    text: root.minimized ? qsTr("Restore") : qsTr("Minimise (double-click the title bar)")
                    visible: parent.hovered
                }
            }
            IconButton {
                theme: root.theme
                objectName: "toolWindowCloseButton"
                Layout.alignment: Qt.AlignVCenter
                glyph: "close"
                Accessible.name: qsTr("Close %1").arg(root.title)
                onClicked: root.closeRequested()
                ThemedToolTip {
                    theme: root.theme
                    text: qsTr("Close")
                    visible: parent.hovered
                }
            }
        }

        Rectangle {
            visible: !root.minimized
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            height: 1
            color: root.theme ? root.theme.border : "transparent"
        }
    }

    // The panel, scrolled when the workspace is too short for all of it.
    Flickable {
        id: scroller
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        visible: !root.minimized
        clip: true
        contentWidth: width
        contentHeight: root.panelHeight
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        ScrollBar.vertical: ScrollBar { policy: scroller.interactive ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

        Item {
            id: body
            width: scroller.width
            height: root.panelHeight
        }
    }
}
