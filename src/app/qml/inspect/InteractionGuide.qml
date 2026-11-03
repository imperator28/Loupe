import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// View > Interaction: every gesture and shortcut, drawn the way they are made -- keys as key
// caps (with the macOS symbols on a Mac), mouse buttons and trackpad gestures as pictures --
// rather than spelled out, so a row can be read at a glance.
Dialog {
    id: root

    objectName: "interactionGuidePopup"
    property QtObject theme
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(720, Overlay.overlay.width - root.theme.spacing4 * 2)
    height: Math.min(640, Overlay.overlay.height - root.theme.spacing4 * 2)
    padding: root.theme.spacing4
    closePolicy: Popup.CloseOnEscape
    header: null
    footer: null
    background: ElevatedPanel { theme: root.theme; cornerRadius: root.theme.radius4 }

    readonly property bool mac: Qt.platform.os === "osx"

    // A key's label as written on the keyboard in front of the user.
    function keyLabel(name) {
        if (name === "Mod") return mac ? "⌘" : "Ctrl"
        if (name === "Shift") return mac ? "⇧" : "Shift"
        if (name === "Alt") return mac ? "⌥" : "Alt"
        if (name === "Esc") return mac ? "esc" : "Esc"
        return name
    }

    // One key on the keyboard: a raised cap.
    component KeyCap: Rectangle {
        property string key
        implicitWidth: Math.max(26, capLabel.implicitWidth + 14)
        implicitHeight: 26
        radius: 5
        color: root.theme.surfaceRaised
        border.color: root.theme.borderStrong
        // The cap's lower lip, so it reads as a key and not a button on screen.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            anchors.bottomMargin: 1
            height: 3
            radius: 4
            color: root.theme.control
        }
        Label {
            id: capLabel
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            text: root.keyLabel(parent.key)
            color: root.theme.foreground
            font.pixelSize: root.theme.fontCaption
            font.bold: true
        }
    }

    // A pictured gesture: the mouse or trackpad, and what to do with it.
    component Gesture: RowLayout {
        property string icon
        property string verb
        // As tall as a key cap, so a row of keys and pictures sits on one line.
        implicitHeight: 26
        spacing: 3
        ToolIcon { name: parent.icon; color: root.theme.foreground }
        Label {
            visible: parent.verb.length > 0
            text: parent.verb
            color: root.theme.foreground
            font.pixelSize: root.theme.fontCaption
        }
    }

    // One row: its input as a sequence of tokens, then what it does. A token is "k:Name" for
    // a key, "g:icon:verb" for a pictured gesture, "+" or "/" between them, or "t:text".
    component GuideRow: RowLayout {
        id: row
        required property var input
        required property string action
        Layout.fillWidth: true
        spacing: root.theme.spacing3

        Flow {
            Layout.preferredWidth: 232
            Layout.minimumWidth: 200
            Layout.alignment: Qt.AlignVCenter
            spacing: 4
            Repeater {
                model: row.input
                delegate: Loader {
                    required property string modelData
                    readonly property var parts: modelData.split(":")
                    sourceComponent: parts[0] === "k" ? keyToken : parts[0] === "g" ? gestureToken : textToken
                    Component {
                        id: keyToken
                        KeyCap { key: parts[1] }
                    }
                    Component {
                        id: gestureToken
                        Gesture { icon: parts[1]; verb: parts.length > 2 ? parts[2] : "" }
                    }
                    Component {
                        id: textToken
                        Label {
                            height: 26
                            verticalAlignment: Text.AlignVCenter
                            text: parts[0] === "t" ? parts.slice(1).join(":") : modelData
                            color: root.theme.muted
                            font.pixelSize: root.theme.fontCaption
                        }
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: row.action
            color: root.theme.foreground
            font.pixelSize: root.theme.fontCaption
            wrapMode: Text.Wrap
        }
    }

    component GuideSection: ColumnLayout {
        required property string title
        default property alias rows: sectionRows.data
        Layout.fillWidth: true
        spacing: root.theme.spacing2
        Label {
            text: parent.title
            color: root.theme.foreground
            font.bold: true
            font.pixelSize: root.theme.fontSection
        }
        ColumnLayout {
            id: sectionRows
            Layout.fillWidth: true
            spacing: root.theme.spacing2
        }
    }

    contentItem: ColumnLayout {
        width: root.availableWidth
        spacing: root.theme.spacing3

        Label {
            text: qsTr("Interaction")
            color: root.theme.foreground
            font.bold: true
            font.pixelSize: root.theme.fontTitle
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Navigation, selection, and tool controls")
            color: root.theme.muted
            font.pixelSize: root.theme.fontBody
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.theme.border
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: scroll.availableWidth - root.theme.spacing3
                spacing: root.theme.spacing4

                GuideSection {
                    title: qsTr("Mouse and trackpad")
                    GuideRow { input: ["g:mouseLeft:drag"]; action: qsTr("Rotate around the point under the cursor") }
                    GuideRow { input: ["g:mouseMiddle:drag", "/", "k:Shift", "+", "g:mouseLeft:drag"]; action: qsTr("Pan") }
                    GuideRow { input: ["k:Alt", "+", "g:mouseLeft:drag"]; action: qsTr("Rotate in the screen's plane") }
                    GuideRow { input: ["g:mouseMiddle:scroll", "/", "g:trackpadPinch:pinch"]; action: qsTr("Zoom") }
                    GuideRow { input: ["g:trackpadScroll:two fingers"]; action: qsTr("Pan") }
                    GuideRow { input: ["g:mouseRight:click"]; action: qsTr("Actions for what is under the cursor") }
                }

                GuideSection {
                    title: qsTr("Selection and view")
                    GuideRow { input: ["g:mouseLeft:click a part"]; action: qsTr("Select it") }
                    GuideRow { input: ["k:Shift", "/", "k:Mod", "+", "g:mouseLeft:click"]; action: qsTr("Add a part to the selection, or take it out") }
                    GuideRow { input: ["g:mouseLeft:click empty space"]; action: qsTr("Clear the selection") }
                    GuideRow { input: ["g:viewCube:click a face"]; action: qsTr("Turn to a standard view") }
                    GuideRow { input: ["k:1", "/", "k:2", "/", "k:3"]; action: qsTr("Solid / Solid + Edges / Edges Only") }
                }

                GuideSection {
                    title: qsTr("Keyboard")
                    GuideRow { input: ["k:F", "/", "k:Shift", "+", "k:F"]; action: qsTr("Fit the assembly / fit the selection") }
                    GuideRow { input: ["k:I"]; action: qsTr("Isolate the selection, or show everything again") }
                    GuideRow { input: ["k:H"]; action: qsTr("Hide the selection") }
                    GuideRow { input: ["k:T", "/", "k:Shift", "+", "k:T"]; action: qsTr("Make the selection transparent / make everything else transparent") }
                    GuideRow { input: ["k:Mod", "+", "k:F"]; action: qsTr("Find a part in the assembly tree") }
                    GuideRow { input: ["k:Mod", "+", "k:8"]; action: qsTr("Look straight at the face under the cursor (again: its other side)") }
                    GuideRow { input: ["k:Mod", "+", "k:Shift", "+", "k:H"]; action: qsTr("Show all") }
                    GuideRow { input: ["k:M", "/", "k:S", "/", "k:E"]; action: qsTr("Measure / Section / Exploded view") }
                    GuideRow { input: ["k:Esc"]; action: qsTr("Cancel a drag, restore visibility, or close a tool") }
                }

                GuideSection {
                    title: qsTr("Exploded view")
                    GuideRow { input: ["g:handleArrow:arrow"]; action: qsTr("Drag to slide the selection along that axis") }
                    GuideRow { input: ["g:handleBall:ball"]; action: qsTr("Drag to move the selection anywhere in the screen's plane") }
                    GuideRow { input: ["g:handleRing:ring"]; action: qsTr("Drag to turn the selection") }
                    GuideRow { input: ["k:Shift", "t:while dragging"]; action: qsTr("Move or turn freely, without snapping") }
                    GuideRow { input: ["k:Mod", "+", "k:Z", "/", "k:Mod", "+", "k:Shift", "+", "k:Z"]; action: qsTr("Undo / redo a move") }
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
            Item { Layout.fillWidth: true }
            ThemedButton {
                text: qsTr("Close")
                theme: root.theme
                onClicked: root.close()
            }
        }
    }
}
