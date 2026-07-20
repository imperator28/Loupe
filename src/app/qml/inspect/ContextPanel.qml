import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Loupe.App

Rectangle {
    id: root
    property var controller
    property QtObject theme
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"
    readonly property color subduedForeground: theme && theme.dark ? "#aeb8c2" : "#53636c"
    color: root.theme.surfaceRaised

    function materialIndex(materialId) {
        const options = controller ? controller.materials.materials : []
        for (let index = 0; index < options.length; ++index) {
            if (options[index].value === materialId) return index
        }
        return 0
    }

    function customMaterialIndex(materialId) {
        const options = controller ? controller.materials.materials : []
        let customIndex = 0
        for (let index = 0; index < options.length; ++index) {
            if (!options[index].custom) continue
            if (options[index].value === materialId) return customIndex
            ++customIndex
        }
        return 0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Label {
            text: controller && controller.activeNodeId !== "" ? qsTr("Component properties") : qsTr("Document properties")
            color: root.foreground
            font.bold: true
            Layout.fillWidth: true
        }
        Label {
            visible: controller && controller.activeNodeId !== ""
            text: qsTr("Surface area  %1 mm²").arg(Number(controller ? controller.activeSurfaceAreaMm2 : 0).toLocaleString(Qt.locale(), 'f', 2))
            color: root.subduedForeground
            Layout.fillWidth: true
        }
        Label {
            visible: controller && controller.activeNodeId !== ""
            text: qsTr("Volume  %1 mm³").arg(Number(controller ? controller.activeVolumeMm3 : 0).toLocaleString(Qt.locale(), 'f', 2))
            color: root.subduedForeground
            Layout.fillWidth: true
        }

        Label { visible: controller && controller.activeNodeId !== ""; text: qsTr("Assignment scope"); color: root.foreground }
        ThemedComboBox {
            id: assignmentScope
            theme: root.theme
            visible: controller && controller.activeNodeId !== ""
            Layout.fillWidth: true
            model: [{ text: qsTr("This occurrence"), value: "occurrence" }, { text: qsTr("All instances"), value: "definition" }]
            textRole: "text"
            valueRole: "value"
        }

        Label { visible: controller && controller.activeNodeId !== ""; text: qsTr("Material"); color: root.foreground }
        ThemedComboBox {
            id: materialSelector
            theme: root.theme
            visible: controller && controller.activeNodeId !== ""
            Layout.fillWidth: true
            textRole: "text"
            valueRole: "value"
            model: controller ? controller.materials.materials : []
            currentIndex: root.materialIndex(controller ? controller.activeMaterialId : "")
            delegate: ItemDelegate {
                required property var modelData
                width: materialSelector.width
                contentItem: RowLayout {
                    spacing: 8
                    Rectangle { implicitWidth: 12; implicitHeight: 12; color: modelData.color; border.color: "#6f7d89"; visible: modelData.value !== "" }
                    Label { text: modelData.text; color: root.foreground; Layout.fillWidth: true }
                }
            }
            onActivated: function(index) {
                if (controller) controller.assignActiveMaterial(currentValue, assignmentScope.currentValue)
            }
        }
        RowLayout {
            visible: controller && controller.activeNodeId !== ""
            Layout.fillWidth: true
            ThemedButton {
                Layout.fillWidth: true
                theme: root.theme
                text: qsTr("Add material")
                onClicked: materialDialog.open()
            }
            ThemedButton {
                theme: root.theme
                text: qsTr("Manage")
                onClicked: materialManager.open()
            }
        }
        Label {
            visible: controller && controller.activeMaterialId !== ""
            text: qsTr("Estimated mass  %1 kg").arg(Number(controller ? controller.estimatedMassKg : 0).toLocaleString(Qt.locale(), 'f', 3))
            color: root.theme.accent
            font.bold: true
            Layout.fillWidth: true
        }

        Label { visible: controller && controller.activeNodeId !== ""; text: qsTr("Appearance"); color: root.foreground }
        RowLayout {
            visible: controller && controller.activeNodeId !== ""
            Layout.fillWidth: true
            ThemedButton {
                id: appearanceSwatch
                theme: root.theme
                Layout.minimumWidth: 42
                Layout.minimumHeight: 32
                text: ""
                background: Rectangle {
                    radius: 3
                    color: controller ? controller.activeResolvedAppearanceColor : "#B8C2CC"
                    border.color: "#d7e0e8"
                }
                onClicked: appearanceDialog.open()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Set body color")
            }
            ThemedButton {
                theme: root.theme
                text: qsTr("Clear")
                enabled: controller && controller.activeAppearanceColor !== ""
                onClicked: controller.clearActiveAppearanceColor(assignmentScope.currentValue)
            }
        }
        Item { Layout.fillHeight: true }
    }

    ColorDialog {
        id: appearanceDialog
        title: qsTr("Body color")
        selectedColor: controller ? controller.activeResolvedAppearanceColor : "#B8C2CC"
        onAccepted: if (controller) controller.setActiveAppearanceColor(selectedColor, assignmentScope.currentValue)
    }

    Dialog {
        id: materialDialog
        title: qsTr("New material")
        modal: true
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok | Dialog.Cancel
        palette.window: root.theme.surfaceRaised
        palette.windowText: root.foreground
        palette.base: root.theme.surface
        palette.text: root.foreground
        palette.button: root.theme.control
        palette.buttonText: root.foreground
        background: Rectangle { color: root.theme.surfaceRaised; border.color: root.theme.border; radius: 4 }
        property color materialColor: "#67d5c0"
        onAccepted: {
            if (!controller) return
            const id = controller.materials.addMaterial(materialName.text, Number(materialDensity.text), materialColor)
            if (id !== "") controller.assignActiveMaterial(id, assignmentScope.currentValue)
        }
        contentItem: ColumnLayout {
            width: 280
            spacing: 10
            Label { text: qsTr("Name"); color: root.foreground }
            ThemedTextField { id: materialName; theme: root.theme; Layout.fillWidth: true; placeholderText: qsTr("Material name") }
            Label { text: qsTr("Density (g/cm³)"); color: root.foreground }
            ThemedTextField { id: materialDensity; theme: root.theme; Layout.fillWidth: true; inputMethodHints: Qt.ImhFormattedNumbersOnly; text: "1.0" }
            Label { text: qsTr("Color"); color: root.foreground }
            ThemedButton {
                theme: root.theme
                Layout.preferredWidth: 44
                Layout.preferredHeight: 32
                text: ""
                background: Rectangle { radius: 3; color: materialDialog.materialColor; border.color: "#d7e0e8" }
                onClicked: materialColorDialog.open()
            }
        }
    }

    ColorDialog {
        id: materialColorDialog
        title: qsTr("Material color")
        selectedColor: materialDialog.materialColor
        onAccepted: materialDialog.materialColor = selectedColor
    }

    Dialog {
        id: materialManager
        title: qsTr("Custom materials")
        modal: true
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Close
        palette.window: root.theme.surfaceRaised
        palette.windowText: root.foreground
        palette.base: root.theme.surface
        palette.text: root.foreground
        palette.button: root.theme.control
        palette.buttonText: root.foreground
        background: Rectangle { color: root.theme.surfaceRaised; border.color: root.theme.border; radius: 4 }
        property color materialColor: "#67d5c0"
        property var customMaterials: {
            if (!controller) return []
            return controller.materials.materials.filter(function(item) { return item.custom === true })
        }
        function loadMaterial(index) {
            if (index < 0 || index >= customMaterials.length) {
                managerName.text = ""
                managerDensity.text = "1.0"
                materialColor = "#67d5c0"
                return
            }
            const material = customMaterials[index]
            managerName.text = material.text
            managerDensity.text = Number(material.density).toString()
            materialColor = material.color
        }
        onOpened: {
            materialPicker.currentIndex = root.customMaterialIndex("")
            loadMaterial(materialPicker.currentIndex)
        }
        contentItem: ColumnLayout {
            width: 300
            spacing: 10
            Label { text: qsTr("Custom material"); color: root.foreground }
            ThemedComboBox {
                id: materialPicker
                theme: root.theme
                Layout.fillWidth: true
                model: materialManager.customMaterials
                textRole: "text"
                valueRole: "value"
                onActivated: materialManager.loadMaterial(currentIndex)
            }
            Label {
                visible: materialManager.customMaterials.length === 0
                text: qsTr("No custom materials yet")
                color: root.subduedForeground
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label { text: qsTr("Name"); color: root.foreground }
            ThemedTextField { id: managerName; theme: root.theme; Layout.fillWidth: true; enabled: materialManager.customMaterials.length > 0 }
            Label { text: qsTr("Density (g/cm³)"); color: root.foreground }
            ThemedTextField { id: managerDensity; theme: root.theme; Layout.fillWidth: true; enabled: materialManager.customMaterials.length > 0; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            Label { text: qsTr("Color"); color: root.foreground }
            ThemedButton {
                theme: root.theme
                enabled: materialManager.customMaterials.length > 0
                Layout.preferredWidth: 44
                Layout.preferredHeight: 32
                text: ""
                background: Rectangle { radius: 3; color: materialManager.materialColor; border.color: "#d7e0e8" }
                onClicked: managerColorDialog.open()
            }
            RowLayout {
                Layout.fillWidth: true
                ThemedButton {
                    theme: root.theme
                    text: qsTr("Save changes")
                    enabled: materialManager.customMaterials.length > 0
                    onClicked: {
                        if (!controller) return
                        const material = materialManager.customMaterials[materialPicker.currentIndex]
                        controller.materials.updateMaterial(material.value, managerName.text, Number(managerDensity.text), materialManager.materialColor)
                    }
                }
                Item { Layout.fillWidth: true }
                ThemedButton {
                    theme: root.theme
                    text: qsTr("Delete")
                    enabled: materialManager.customMaterials.length > 0
                    onClicked: deleteMaterialDialog.open()
                }
            }
        }
    }

    ColorDialog {
        id: managerColorDialog
        title: qsTr("Material color")
        selectedColor: materialManager.materialColor
        onAccepted: materialManager.materialColor = selectedColor
    }

    Dialog {
        id: deleteMaterialDialog
        title: qsTr("Delete custom material?")
        modal: true
        width: 340
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Yes | Dialog.No
        palette.window: root.theme.surfaceRaised
        palette.windowText: root.foreground
        palette.base: root.theme.surface
        palette.text: root.foreground
        palette.button: root.theme.control
        palette.buttonText: root.foreground
        background: Rectangle { color: root.theme.surfaceRaised; border.color: root.theme.border; radius: 4 }
        contentItem: Label {
            width: deleteMaterialDialog.availableWidth
            text: qsTr("Components using this material will return to their next available appearance.")
            wrapMode: Text.WordWrap
            color: root.foreground
        }
        onAccepted: {
            if (!controller || materialPicker.currentIndex < 0) return
            const material = materialManager.customMaterials[materialPicker.currentIndex]
            controller.materials.removeMaterial(material.value)
            materialManager.loadMaterial(0)
        }
    }
}
