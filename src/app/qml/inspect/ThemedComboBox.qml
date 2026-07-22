import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Shapes

ComboBox {
    id: control

    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color mutedForeground: theme ? theme.muted : "transparent"
    readonly property color accentFill: theme ? theme.accent : "transparent"
    readonly property color accentForegroundColor: theme && theme.accentForeground !== undefined ? theme.accentForeground : foreground
    readonly property color popupFill: theme && theme.surface3 !== undefined ? theme.surface3
                                                                             : theme ? theme.surfaceRaised : "transparent"
    readonly property int cornerRadius: theme && theme.radius2 !== undefined ? theme.radius2 : 6
    readonly property int popupRadius: theme && theme.radius3 !== undefined ? theme.radius3 : 8
    readonly property int fadeDuration: theme && theme.durInstant !== undefined ? theme.durInstant : 0
    readonly property int ringWidth: theme && theme.focusRingWidth !== undefined ? theme.focusRingWidth : 2
    palette.text: foreground
    palette.buttonText: foreground
    palette.windowText: foreground
    palette.highlightedText: foreground

    implicitHeight: theme && theme.controlHeight !== undefined ? theme.controlHeight : 30
    leftPadding: 10
    rightPadding: 28

    contentItem: Text {
        text: control.displayText
        color: control.enabled ? control.foreground : control.mutedForeground
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    // A proper stroked chevron (design.md's icon language: 1.5 px stroke,
    // round joins) rather than a text glyph, which renders inconsistently
    // across fonts and reads as too small/subtle at typical control sizes.
    indicator: Shape {
        x: control.width - width - 10
        y: (control.height - height) / 2
        width: 11
        height: 7
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: control.mutedForeground
            strokeWidth: 1.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: "M 1 1.5 L 5.5 6 L 10 1.5" }
        }
    }

    background: Item {
        implicitWidth: 110
        implicitHeight: control.implicitHeight

        Rectangle {
            anchors.fill: parent
            radius: control.cornerRadius
            color: control.down || control.popup.visible
                   ? (control.theme && control.theme.dark ? Qt.lighter(control.theme.control, 1.28) : control.theme ? Qt.darker(control.theme.control, 1.10) : "transparent")
                   : control.hovered
                     ? (control.theme && control.theme.dark ? Qt.lighter(control.theme.control, 1.16) : control.theme ? Qt.darker(control.theme.control, 1.05) : "transparent")
                     : control.theme ? control.theme.control : "transparent"
            border.color: control.theme ? control.theme.border : "transparent"
            border.width: 1
            opacity: control.enabled ? 1 : 0.45

            Behavior on color {
                ColorAnimation { duration: control.fadeDuration }
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -(control.ringWidth + 1)
            radius: control.cornerRadius + control.ringWidth + 1
            color: "transparent"
            border.color: control.accentFill
            border.width: control.ringWidth
            visible: control.visualFocus
        }
    }

    delegate: ItemDelegate {
        required property var modelData
        required property int index
        width: control.width - 8
        x: 4
        height: 26
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.textRole.length > 0 && modelData && modelData[control.textRole] !== undefined
                  ? modelData[control.textRole] : modelData
            color: parent.highlighted ? control.accentForegroundColor : control.foreground
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: control.theme && control.theme.radius1 !== undefined ? control.theme.radius1 : 4
            color: parent.highlighted ? control.accentFill : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 280)
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            radius: control.popupRadius
            color: control.popupFill
            border.color: control.theme ? control.theme.border : "transparent"
        }

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: control.theme && control.theme.durFast !== undefined ? control.theme.durFast : 0
                easing.type: Easing.BezierSpline
                easing.bezierCurve: control.theme && control.theme.easeEnter !== undefined
                                    ? control.theme.easeEnter : [0.16, 1.0, 0.30, 1.0, 1.0, 1.0]
            }
        }
    }
}
