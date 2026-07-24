import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Loupe.App

ElevatedPanel {
    id: root
    property var controller
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"

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
        anchors.margins: root.theme.spacing3
        spacing: root.theme.spacing2 + 2

        Label {
            text: !controller || controller.selectedNodeCount === 0 ? qsTr("Document properties")
                  : controller.selectedNodeCount === 1 ? qsTr("Component properties")
                                                       : qsTr("Selection properties  %1 bodies").arg(controller.selectedNodeCount)
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
                    Rectangle { implicitWidth: 12; implicitHeight: 12; color: modelData.color; border.color: root.theme.border; visible: modelData.value !== "" }
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
            visible: controller && controller.selectedNodeCount > 0
            text: qsTr("Estimated mass  %1").arg(controller ? controller.estimatedMassLabel : "")
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
                    color: controller ? controller.activeResolvedAppearanceColor : root.theme.neutralBody
                    border.color: root.theme.border
                }
                onClicked: appearanceDialog.open()
                ThemedToolTip {
                    theme: root.theme
                    text: qsTr("Set body color")
                    visible: parent.hovered
                }
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

    ThemedColorDialog {
        id: appearanceDialog
        theme: root.theme
        title: qsTr("Body color")
        selectedColor: controller ? controller.activeResolvedAppearanceColor : root.theme.neutralBody
        onAccepted: if (controller) controller.setActiveAppearanceColor(pickedColor, assignmentScope.currentValue)
    }

    Dialog {
        id: materialDialog
        objectName: "newMaterialDialog"
        title: qsTr("New material")
        modal: true
        width: Math.min(420, Overlay.overlay.width - root.theme.spacing4 * 2)
        padding: root.theme.spacing4
        closePolicy: Popup.CloseOnEscape
        anchors.centerIn: Overlay.overlay
        palette {
            window: root.theme.surfaceRaised
            windowText: root.foreground
            base: root.theme.surface
            text: root.foreground
            button: root.theme.control
            buttonText: root.foreground
        }
        background: ElevatedPanel { theme: root.theme; cornerRadius: root.theme.radius4 }
        header: null
        footer: null
        property color materialColor: root.theme.accent
        readonly property bool validInput: materialName.text.trim().length > 0
                                                   && Number(materialDensity.text) > 0

        onOpened: {
            materialName.clear()
            materialDensity.text = "1.0"
            materialColor = root.theme.accent
            Qt.callLater(function() { materialName.forceActiveFocus() })
        }
        onAccepted: {
            if (!controller) return
            const id = controller.materials.addMaterial(materialName.text.trim(), Number(materialDensity.text), materialColor)
            if (id !== "") controller.assignActiveMaterial(id, assignmentScope.currentValue)
        }
        contentItem: ColumnLayout {
            width: materialDialog.availableWidth
            spacing: root.theme.spacing3

            Label {
                text: materialDialog.title
                color: root.foreground
                font.bold: true
                font.pixelSize: root.theme.fontTitle
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("Add a reusable material to the library and apply it to the selected component.")
                color: root.subduedForeground
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            GridLayout {
                columns: 2
                columnSpacing: root.theme.spacing3
                rowSpacing: root.theme.spacing2
                Layout.fillWidth: true

                Label { text: qsTr("Name"); color: root.foreground; Layout.preferredWidth: 92 }
                ThemedTextField {
                    id: materialName
                    theme: root.theme
                    placeholderText: qsTr("Material name")
                    Layout.fillWidth: true
                }
                Label { text: qsTr("Density"); color: root.foreground }
                ThemedTextField {
                    id: materialDensity
                    theme: root.theme
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    placeholderText: qsTr("g/cm³")
                    text: "1.0"
                    Layout.fillWidth: true
                }
                Label { text: qsTr("Color"); color: root.foreground }
                ThemedColorButton {
                    theme: root.theme
                    selectedColor: materialDialog.materialColor
                    Layout.fillWidth: true
                    onClicked: materialColorDialog.open()
                }
            }

            Rectangle {
                color: root.theme.border
                implicitHeight: 1
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: root.theme.spacing2
                Layout.fillWidth: true

                Item { Layout.fillWidth: true }
                ThemedButton {
                    theme: root.theme
                    text: qsTr("Cancel")
                    onClicked: materialDialog.reject()
                }
                ThemedButton {
                    theme: root.theme
                    primary: true
                    text: qsTr("Create")
                    enabled: materialDialog.validInput
                    onClicked: materialDialog.accept()
                }
            }
        }
    }

    ThemedColorDialog {
        id: materialColorDialog
        theme: root.theme
        title: qsTr("Material color")
        selectedColor: materialDialog.materialColor
        onAccepted: materialDialog.materialColor = pickedColor
    }

    Dialog {
        id: materialManager
        objectName: "materialManagerDialog"
        title: qsTr("Custom materials")
        modal: true
        width: Math.min(460, Overlay.overlay.width - root.theme.spacing4 * 2)
        padding: root.theme.spacing4
        closePolicy: Popup.CloseOnEscape
        anchors.centerIn: Overlay.overlay
        palette {
            window: root.theme.surfaceRaised
            windowText: root.foreground
            base: root.theme.surface
            text: root.foreground
            button: root.theme.control
            buttonText: root.foreground
        }
        background: ElevatedPanel { theme: root.theme; cornerRadius: root.theme.radius4 }
        header: null
        footer: null
        property color materialColor: root.theme.accent
        property var customMaterials: {
            if (!controller) return []
            return controller.materials.materials.filter(function(item) { return item.custom === true })
        }
        function loadMaterial(index) {
            if (index < 0 || index >= customMaterials.length) {
                managerName.text = ""
                managerDensity.text = "1.0"
                materialColor = root.theme.accent
                return
            }
            const material = customMaterials[index]
            managerName.text = material.text
            managerDensity.text = Number(material.density).toString()
            materialColor = material.color
        }
        onOpened: {
            materialPicker.currentIndex = materialManager.customMaterials.length > 0 ? 0 : -1
            loadMaterial(materialPicker.currentIndex)
        }
        contentItem: ColumnLayout {
            width: materialManager.availableWidth
            spacing: root.theme.spacing3

            Label {
                text: materialManager.title
                color: root.foreground
                font.bold: true
                font.pixelSize: root.theme.fontTitle
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("Edit or remove materials saved in your local library.")
                color: root.subduedForeground
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Label { text: qsTr("Material"); color: root.foreground }
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
                text: qsTr("No custom materials yet. Create one from the component properties panel.")
                color: root.subduedForeground
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            GridLayout {
                columns: 2
                columnSpacing: root.theme.spacing3
                rowSpacing: root.theme.spacing2
                enabled: materialManager.customMaterials.length > 0
                Layout.fillWidth: true

                Label { text: qsTr("Name"); color: root.foreground; Layout.preferredWidth: 92 }
                ThemedTextField {
                    id: managerName
                    theme: root.theme
                    Layout.fillWidth: true
                }
                Label { text: qsTr("Density"); color: root.foreground }
                ThemedTextField {
                    id: managerDensity
                    theme: root.theme
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    placeholderText: qsTr("g/cm³")
                    Layout.fillWidth: true
                }
                Label { text: qsTr("Color"); color: root.foreground }
                ThemedColorButton {
                    theme: root.theme
                    selectedColor: materialManager.materialColor
                    Layout.fillWidth: true
                    onClicked: managerColorDialog.open()
                }
            }

            Rectangle {
                color: root.theme.border
                implicitHeight: 1
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: root.theme.spacing2
                Layout.fillWidth: true

                ThemedButton {
                    theme: root.theme
                    destructive: true
                    text: qsTr("Delete")
                    enabled: materialManager.customMaterials.length > 0
                    onClicked: deleteMaterialDialog.open()
                }
                Item { Layout.fillWidth: true }
                ThemedButton {
                    theme: root.theme
                    text: qsTr("Close")
                    onClicked: materialManager.close()
                }
                ThemedButton {
                    theme: root.theme
                    primary: true
                    text: qsTr("Save changes")
                    enabled: materialManager.customMaterials.length > 0
                             && managerName.text.trim().length > 0
                             && Number(managerDensity.text) > 0
                    onClicked: {
                        if (!controller) return
                        const material = materialManager.customMaterials[materialPicker.currentIndex]
                        controller.materials.updateMaterial(material.value, managerName.text.trim(), Number(managerDensity.text), materialManager.materialColor)
                    }
                }
            }
        }
    }

    ThemedColorDialog {
        id: managerColorDialog
        theme: root.theme
        title: qsTr("Material color")
        selectedColor: materialManager.materialColor
        onAccepted: materialManager.materialColor = pickedColor
    }

    Dialog {
        id: deleteMaterialDialog
        title: qsTr("Delete custom material?")
        modal: true
        width: 340
        anchors.centerIn: Overlay.overlay
        palette.window: root.theme.surfaceRaised
        palette.windowText: root.foreground
        palette.base: root.theme.surface
        palette.text: root.foreground
        palette.button: root.theme.control
        palette.buttonText: root.foreground
        background: ElevatedPanel { theme: root.theme; cornerRadius: root.theme.radius4 }
        header: null
        footer: Item {
            implicitHeight: deleteDialogFooter.implicitHeight + root.theme.spacing3 * 2
            RowLayout {
                id: deleteDialogFooter
                anchors.fill: parent
                anchors.margins: root.theme.spacing3
                spacing: root.theme.spacing2
                Item { Layout.fillWidth: true }
                ThemedButton { theme: root.theme; text: qsTr("Cancel"); onClicked: deleteMaterialDialog.reject() }
                ThemedButton { theme: root.theme; destructive: true; text: qsTr("Delete"); onClicked: deleteMaterialDialog.accept() }
            }
        }
        contentItem: ColumnLayout {
            width: deleteMaterialDialog.availableWidth
            spacing: 10
            Label { text: deleteMaterialDialog.title; color: root.foreground; font.bold: true; font.pixelSize: root.theme.fontTitle; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label {
                text: qsTr("Components using this material will return to their next available appearance.")
                wrapMode: Text.WordWrap
                color: root.foreground
                Layout.fillWidth: true
            }
        }
        onAccepted: {
            if (!controller || materialPicker.currentIndex < 0) return
            const removedIndex = materialPicker.currentIndex
            const material = materialManager.customMaterials[materialPicker.currentIndex]
            controller.materials.removeMaterial(material.value)
            Qt.callLater(function() {
                const nextIndex = Math.min(removedIndex, materialManager.customMaterials.length - 1)
                materialPicker.currentIndex = nextIndex
                materialManager.loadMaterial(nextIndex)
            })
        }
    }
}
