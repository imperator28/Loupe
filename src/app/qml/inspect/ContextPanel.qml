import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    property var controller
    color: "#182027"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Label {
            text: controller && controller.activeNodeId !== "" ? qsTr("Component properties") : qsTr("Document properties")
            color: "#e6edf3"
            font.bold: true
            Layout.fillWidth: true
        }
        Label {
            visible: controller && controller.activeNodeId !== ""
            text: qsTr("Surface area  %1 mm²").arg(Number(controller ? controller.activeSurfaceAreaMm2 : 0).toLocaleString(Qt.locale(), 'f', 2))
            color: "#9aa8b5"
            Layout.fillWidth: true
        }
        Label {
            visible: controller && controller.activeNodeId !== ""
            text: qsTr("Volume  %1 mm³").arg(Number(controller ? controller.activeVolumeMm3 : 0).toLocaleString(Qt.locale(), 'f', 2))
            color: "#9aa8b5"
            Layout.fillWidth: true
        }
        ComboBox {
            id: materialSelector
            visible: controller && controller.activeNodeId !== ""
            Layout.fillWidth: true
            textRole: "text"
            valueRole: "value"
            model: [
                { text: qsTr("Material — unassigned"), value: "" },
                { text: qsTr("Aluminum 6061"), value: "aluminum-6061" },
                { text: qsTr("Carbon steel"), value: "steel-carbon" },
                { text: qsTr("Stainless steel 304"), value: "stainless-304" },
                { text: qsTr("ABS"), value: "abs" }
            ]
            currentIndex: !controller || controller.activeMaterialId === "" ? 0
                : controller.activeMaterialId === "aluminum-6061" ? 1
                : controller.activeMaterialId === "steel-carbon" ? 2
                : controller.activeMaterialId === "stainless-304" ? 3
                : 4
            onActivated: index => {
                if (index > 0 && controller) controller.assignActiveMaterial(currentValue)
            }
        }
        Label {
            visible: controller && controller.activeMaterialId !== ""
            text: qsTr("Estimated mass  %1 kg").arg(Number(controller ? controller.estimatedMassKg : 0).toLocaleString(Qt.locale(), 'f', 3))
            color: "#78c4d4"
            font.bold: true
            Layout.fillWidth: true
        }
        Item { Layout.fillHeight: true }
    }
}
