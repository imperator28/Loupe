import QtQuick
import QtTest
import "../../src/app/qml"
import "../../src/app/qml/export"

Item {
    id: root
    width: 420
    height: 600

    Theme { id: appTheme; mode: "Light" }

    // The export draft as the bucket sees it.
    QtObject {
        id: draft
        property var bucketRows: [
            { nodeId: "bracket", name: "Bracket", filename: "bracket.step", status: "ok", error: "", copyCount: 1 },
            { nodeId: "cover", name: "Cover", filename: "cover.step", status: "ok", error: "", copyCount: 2 }
        ]
        property int checkedCount: 2
        property int fileCount: 2
        property bool exporting: false
        property string focusedNodeId: ""
        property string hoveredNodeId: ""
        function moveBucketItemById(nodeId, to) {}
        function removeBucketRow(nodeId) {}
        function setFilenameOverride(nodeId, filename) {}
    }

    ExportBucket {
        id: bucket
        anchors.fill: parent
        theme: appTheme
        draft: draft
    }

    SignalSpy { id: selected; target: bucket; signalName: "rowSelected" }

    TestCase {
        name: "ExportBucketTests"
        when: windowShown

        function test_clickingAQueuedRowSelectsThatPart() {
            selected.clear()
            const row = findChild(bucket, "exportBucketRow-cover")
            verify(row !== null)
            verify(!row.focused)
            // On the row itself (its padding), clear of its buttons and its name field.
            mouseClick(row, row.width / 2, 2)
            compare(draft.focusedNodeId, "cover")
            compare(selected.count, 1)
            compare(selected.signalArguments[0][0], "cover")
            verify(row.focused)
            verify(Qt.colorEqual(row.border.color, appTheme.accent))
            verify(!findChild(bucket, "exportBucketRow-bracket").focused)
        }
    }
}
