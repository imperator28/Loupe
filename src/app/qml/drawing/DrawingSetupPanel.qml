import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../inspect" as Inspect

// Configures the candidate drawing, and adds it to the queue.
//
// Ordered so the preview above has already been read by the time the add button is
// reached: the requirement is that nothing is queued sight-unseen.
ColumnLayout {
    id: root

    property QtObject draft
    property QtObject theme
    // Resolves a standard view name against the document up axis, so this panel and the
    // view cube cannot disagree about what "Top" means.
    property QtObject viewResolver
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property var standardViews: [qsTr("Top"), qsTr("Bottom"), qsTr("Front"),
                                          qsTr("Back"), qsTr("Left"), qsTr("Right")]

    signal addRequested()

    function applyStandardView(label) {
        if (!root.draft) return
        if (!root.viewResolver) {
            // Without a resolver there is no defensible mapping from a name to an axis, so
            // nothing is set rather than guessing one.
            return
        }
        const view = root.viewResolver.directionForStandardView(label)
        const up = root.viewResolver.upForStandardView(label)
        root.draft.setCandidateStandardView(label, view.x, view.y, view.z, up.x, up.y, up.z)
    }

    spacing: root.theme.spacing2

    Label {
        text: qsTr("Drawing setup")
        color: root.foreground
        font.bold: true
        font.pixelSize: root.theme.fontTitle
    }

    Label {
        Layout.fillWidth: true
        text: root.draft && root.draft.candidateNodeId.length > 0
              ? qsTr("View")
              : qsTr("Choose a part on the left")
        color: root.theme.muted
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 3
        columnSpacing: root.theme.spacing1
        rowSpacing: root.theme.spacing1

        Repeater {
            model: root.standardViews
            delegate: Inspect.ThemedButton {
                required property string modelData
                objectName: "drawingStandardView-" + modelData
                theme: root.theme
                Layout.fillWidth: true
                text: modelData
                enabled: root.draft && root.draft.candidateNodeId.length > 0 && !root.draft.exporting
                primary: root.draft && root.draft.candidateViewLabel === modelData
                onClicked: root.applyStandardView(modelData)
            }
        }
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("Or click a flat face in the 3D view, or a cube face")
        color: root.theme.muted
        font.pixelSize: root.theme.fontCaption
        wrapMode: Text.Wrap
    }

    Label { text: qsTr("Content"); color: root.theme.muted }
    Inspect.ThemedComboBox {
        id: contentMode
        objectName: "drawingContentMode"
        theme: root.theme
        Layout.fillWidth: true
        enabled: !root.draft || !root.draft.exporting
        // Values, not translations: the controller matches on these strings.
        readonly property var modes: ["Cut contours", "Outer contour only", "Technical view"]
        model: [qsTr("Cut contours"), qsTr("Outer contour only"), qsTr("Technical view")]
        currentIndex: root.draft ? Math.max(0, modes.indexOf(root.draft.candidateContentMode)) : 0
        onActivated: if (root.draft) root.draft.setCandidateContentMode(modes[currentIndex])
        Inspect.ThemedToolTip {
            theme: root.theme
            visible: parent.hovered
            text: contentMode.currentIndex === 1
                  ? qsTr("Only the outline that bounds material. Step and chamfer lines are dropped.")
                  : contentMode.currentIndex === 2
                    ? qsTr("Every visible edge, on separate layers. For reference, not cutting.")
                    : qsTr("Outer profile and through-holes. Closed contours for a cutter.")
        }
    }

    Label { text: qsTr("Scale"); color: root.theme.muted }
    Inspect.ThemedComboBox {
        id: scaleMode
        objectName: "drawingScale"
        theme: root.theme
        Layout.fillWidth: true
        enabled: !root.draft || !root.draft.exporting
        readonly property var ratios: [[1, 1], [1, 2], [1, 5], [1, 10], [2, 1]]
        model: ["1:1", "1:2", "1:5", "1:10", "2:1"]
        currentIndex: {
            if (!root.draft) return 0
            for (let index = 0; index < ratios.length; ++index) {
                if (ratios[index][0] === root.draft.candidateScaleNumerator
                        && ratios[index][1] === root.draft.candidateScaleDenominator) return index
            }
            return 0
        }
        onActivated: if (root.draft)
            root.draft.setCandidateScale(ratios[currentIndex][0], ratios[currentIndex][1])
        Inspect.ThemedToolTip {
            theme: root.theme
            visible: parent.hovered
            text: qsTr("1:1 is exact. Anything else is named in the filename so it cannot be mistaken.")
        }
    }

    Label {
        Layout.fillWidth: true
        objectName: "drawingCandidateStatus"
        text: root.draft ? root.draft.candidateStatus : ""
        color: root.draft && root.draft.candidateValid ? root.theme.muted : root.theme.warning
        wrapMode: Text.Wrap
        font.pixelSize: root.theme.fontCaption
    }

    Inspect.ThemedButton {
        objectName: "drawingAddToQueue"
        theme: root.theme
        primary: true
        Layout.fillWidth: true
        text: qsTr("Add to queue")
        enabled: root.draft && root.draft.candidateValid && !root.draft.exporting
        onClicked: {
            if (!root.draft) return
            root.draft.addCandidateToQueue()
            root.addRequested()
        }
        Inspect.ThemedToolTip {
            theme: root.theme
            visible: parent.hovered
            text: root.draft && root.draft.candidateValid
                  ? qsTr("Queue this view. The direction is fixed now, so later orbiting will not change it.")
                  : qsTr("Choose a part and a view first")
        }
    }
}
