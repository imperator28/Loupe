import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Loupe.App

// A newer Loupe (#3), in the toolbar: a download icon, with a ring around it that fills
// as the download runs. A dot says one is waiting; click for the details and the actions.
// Absent when there is nothing to say.
Item {
    id: root

    property QtObject theme
    // The app's updater; a test passes a stand-in to show any state.
    property QtObject updates: Updates
    // Reads through these, so the bindings stay quiet while the engine tears the updater
    // down before this item.
    readonly property var fallbacks: ({
        state: Updates.Idle,
        userInitiated: false,
        dismissed: true,
        progress: 0,
        message: "",
        releaseUrl: "",
        bytesReceived: 0,
        bytesTotal: 0,
        availableVersion: "",
        currentVersion: ""
    })
    function read(name) { return updates ? updates[name] : fallbacks[name] }
    function call(name) { if (updates) updates[name]() }
    readonly property int state: root.read("state")
    readonly property bool busy: state === Updates.Checking || state === Updates.Installing
    readonly property bool downloading: state === Updates.Downloading
    readonly property bool ready: state === Updates.Ready
    readonly property bool failed: state === Updates.Failed
    // A result of the user's own check, "up to date" included, is shown for a moment.
    readonly property bool noticeShowing: root.read("userInitiated") && noticeTimer.running
                                          && (state === Updates.Checking || state === Updates.UpToDate)
    readonly property bool showing: noticeShowing || downloading || state === Updates.Installing
                                    || (!root.read("dismissed") && (state === Updates.Available || ready || failed))
    readonly property real percent: Math.round(Math.max(0, Math.min(1, root.read("progress"))) * 100)
    readonly property color tint: failed ? theme.error : state === Updates.UpToDate ? theme.muted : theme.accent

    visible: showing
    implicitWidth: visible ? row.implicitWidth : 0
    implicitHeight: 32

    function summary() {
        if (state === Updates.Available) return qsTr("Loupe %1 is available").arg(root.read("availableVersion"))
        if (downloading) return qsTr("Downloading Loupe %1 — %2%").arg(root.read("availableVersion")).arg(percent)
        if (ready) return qsTr("Loupe %1 is ready to install").arg(root.read("availableVersion"))
        return root.read("message")
    }

    function showDetails() { details.open() }

    Timer { id: noticeTimer; interval: 6000 }
    Connections {
        target: Updates
        function onChanged() {
            if (!root.read("userInitiated")) return
            if (root.read("state") === Updates.Checking) noticeTimer.restart()
            // The answer to Help > Check for Updates opens, whatever it is.
            else if (root.read("state") === Updates.UpToDate || root.read("state") === Updates.Available
                     || root.read("state") === Updates.Failed) {
                noticeTimer.restart()
                details.open()
            }
        }
        function onQuitRequested() { Qt.quit() }
    }

    RowLayout {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.theme.spacing1

        IconButton {
            id: button
            objectName: "updateIndicatorButton"
            theme: root.theme
            implicitWidth: 32
            implicitHeight: 32
            iconSize: 18
            glyph: root.state === Updates.UpToDate ? "check" : "download"
            iconColor: root.tint
            Accessible.name: root.summary()
            onClicked: details.opened ? details.close() : details.open()

            // The ring: a faint track, filled clockwise from the top as the download runs.
            Shape {
                anchors.centerIn: parent
                width: 28
                height: 28
                visible: root.downloading || root.ready || root.busy
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: root.theme.border
                    strokeWidth: 2
                    fillColor: "transparent"
                    PathAngleArc { centerX: 14; centerY: 14; radiusX: 13; radiusY: 13; startAngle: 0; sweepAngle: 360 }
                }
            }
            Shape {
                id: progressRing
                objectName: "updateProgressRing"
                anchors.centerIn: parent
                width: 28
                height: 28
                visible: root.downloading || root.ready || root.busy
                preferredRendererType: Shape.CurveRenderer
                readonly property real sweep: root.ready ? 360 : root.busy ? 90 : 360 * root.percent / 100
                ShapePath {
                    strokeColor: root.tint
                    strokeWidth: 2
                    capStyle: ShapePath.RoundCap
                    fillColor: "transparent"
                    PathAngleArc { centerX: 14; centerY: 14; radiusX: 13; radiusY: 13; startAngle: -90; sweepAngle: progressRing.sweep }
                }
                // Checking or installing: no amount to show, so a quarter ring turns.
                RotationAnimator on rotation {
                    running: root.busy && root.visible
                    from: 0
                    to: 360
                    duration: root.theme.durIndeterminateTravel
                    loops: Animation.Infinite
                }
                rotation: 0
            }
            // Something is waiting: a version to fetch, one to install, or a failure to read.
            Rectangle {
                visible: root.state === Updates.Available || root.ready || root.failed
                width: 8
                height: 8
                radius: 4
                x: parent.width - width - 3
                y: 3
                color: root.tint
                border.color: root.theme.surfaceRaised
                border.width: 1.5
            }
            ThemedToolTip {
                theme: root.theme
                text: root.summary()
                visible: button.hovered && !details.opened
            }
        }
        // The amount, beside the ring, while it runs.
        Label {
            objectName: "updatePercent"
            visible: root.downloading
            text: qsTr("%1%").arg(root.percent)
            color: root.theme.muted
            font.pixelSize: root.theme.fontCaption
            font.features: { "tnum": 1 }
        }
    }

    Popup {
        id: details
        objectName: "updateDetails"
        y: root.height + root.theme.spacing1
        x: root.width - width
        width: 320
        padding: root.theme.spacing4
        background: Rectangle {
            color: root.theme.surface3
            radius: root.theme.radius3
            border.color: root.theme.border
        }

        contentItem: ColumnLayout {
            spacing: root.theme.spacing2
            Label {
                Layout.fillWidth: true
                text: root.summary()
                font.bold: true
                wrapMode: Text.WordWrap
                color: root.failed ? root.theme.error : root.theme.foreground
            }
            Label {
                Layout.fillWidth: true
                visible: root.state === Updates.Available || root.downloading || root.ready
                text: qsTr("You have %1.").arg(root.read("currentVersion"))
                color: root.theme.muted
                wrapMode: Text.WordWrap
            }
            ThemedProgressBar {
                theme: root.theme
                Layout.fillWidth: true
                visible: root.downloading
                from: 0
                to: 1
                value: root.read("progress")
            }
            Label {
                Layout.fillWidth: true
                visible: root.downloading
                text: qsTr("%1 of %2 MB").arg((root.read("bytesReceived") / 1048576).toFixed(1))
                                         .arg((root.read("bytesTotal") / 1048576).toFixed(1))
                color: root.theme.muted
                font.pixelSize: root.theme.fontCaption
                font.features: { "tnum": 1 }
            }
            Label {
                Layout.fillWidth: true
                visible: root.ready
                text: qsTr("Loupe restarts with your files closed; the new version opens in the same place.")
                color: root.theme.muted
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: root.theme.spacing2
                spacing: root.theme.spacing2
                ThemedButton {
                    theme: root.theme
                    visible: root.read("releaseUrl").length > 0 && (root.state === Updates.Available || root.failed)
                    text: qsTr("What's New")
                    onClicked: Qt.openUrlExternally(root.read("releaseUrl"))
                }
                Item { Layout.fillWidth: true }
                ThemedButton {
                    theme: root.theme
                    visible: root.state === Updates.Available || root.ready || root.failed
                    text: qsTr("Later")
                    onClicked: { root.call("dismiss"); details.close() }
                }
                ThemedButton {
                    theme: root.theme
                    visible: root.downloading
                    text: qsTr("Cancel")
                    onClicked: root.call("cancel")
                }
                ThemedButton {
                    theme: root.theme
                    visible: root.state === Updates.Available
                    primary: true
                    text: qsTr("Download")
                    onClicked: root.call("download")
                }
                ThemedButton {
                    theme: root.theme
                    visible: root.ready
                    primary: true
                    text: qsTr("Restart to Update")
                    onClicked: root.call("installAndRestart")
                }
            }
        }
    }
}
