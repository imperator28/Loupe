import QtQuick
import QtQuick.Controls.Basic

Menu {
    id: control

    property QtObject theme
    readonly property color fill: theme && theme.surface3 !== undefined ? theme.surface3
                                                                        : theme ? theme.surfaceRaised : "transparent"
    readonly property color outline: theme ? theme.border : "transparent"
    readonly property int cornerRadius: theme && theme.radius3 !== undefined ? theme.radius3 : 8
    readonly property int enterDuration: theme && theme.durStandard !== undefined ? theme.durStandard : 0
    readonly property int exitDuration: Math.round(enterDuration * 0.7)
    readonly property bool travelOk: theme && theme.reducedMotion !== undefined ? !theme.reducedMotion : true

    implicitWidth: Math.max(180, contentItem.implicitWidth + leftPadding + rightPadding)
    topPadding: 4
    bottomPadding: 4
    leftPadding: 4
    rightPadding: 4

    // Used only for content that isn't already an explicit ThemedMenuItem —
    // in practice, a nested Menu (a submenu trigger like "Appearance").
    // Without this, QQC2's own Menu.qml supplies its default, unthemed
    // MenuItem for that row, which is how it ends up with light-theme text
    // color inside an otherwise dark-themed menu.
    delegate: ThemedMenuItem {
        theme: control.theme
    }

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: contentHeight > control.availableHeight
        currentIndex: control.currentIndex
    }

    background: Rectangle {
        radius: control.cornerRadius
        color: control.fill
        border.color: control.outline
        border.width: 1
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: control.enterDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: control.theme && control.theme.easeEnter !== undefined
                                ? control.theme.easeEnter : [0.16, 1.0, 0.30, 1.0, 1.0, 1.0]
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: control.exitDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: control.theme && control.theme.easeExit !== undefined
                                ? control.theme.easeExit : [0.40, 0.0, 1.0, 1.0, 1.0, 1.0]
        }
    }
}
