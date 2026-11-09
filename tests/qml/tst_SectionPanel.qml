import QtQuick
import QtTest
import "../../src/app/qml"
import "../../src/app/qml/inspect"

Item {
    id: root
    width: 800
    height: 600

    Theme { id: appTheme; mode: "Light" }

    Component {
        id: panelComponent
        SectionPanel {}
    }

    // Stands in for SectionController: what the panel reads, and the plane pick it starts.
    Component {
        id: controllerComponent
        QtObject {
            signal changed()
            property string axisName: "Z"
            property real position: 0
            property bool flipped: false
            property bool capEnabled: true
            property bool sliceOnly: false
            property string sliceDisplay: "filled-outline"
            property string sliceBorderColor: ""
            property real sliceBorderWidth: 1.5
            property bool usingSelectedPlane: false
            property string planeLabel: ""
            property bool pickingPlane: false
            property bool usingAxis: false
            property string axisMode: "across"
            property real axisAngle: 0
            function beginPlanePick() { pickingPlane = true }
            function cancelPlanePick() { pickingPlane = false }
        }
    }

    TestCase {
        name: "SectionPanelTests"
        when: windowShown

        function test_sectionPanelHasDesktopSize() {
            let panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            verify(panel.implicitWidth >= 320)
        }

        function test_pickFacePlaneAsksForAFaceAndNamesIt() {
            const controller = createTemporaryObject(controllerComponent, root)
            const panel = createTemporaryObject(panelComponent, root, { theme: appTheme, taskController: controller })
            const button = findChild(panel, "sectionPickFacePlane")
            const source = findChild(panel, "sectionPlaneSource")
            verify(button && source)
            verify(!source.visible)
            button.clicked()
            verify(controller.pickingPlane)
            compare(button.text, "Click a flat face or a hole in the view…")
            button.clicked()
            verify(!controller.pickingPlane)

            controller.usingSelectedPlane = true
            controller.planeLabel = "Flat face on Bracket-2"
            verify(source.visible)
            compare(source.text, "Plane of: Flat face on Bracket-2")
            verify(button.selectedAppearance)
        }
    
        function test_anAxisSectionOffersAcrossOrThroughAtAnAngle() {
            const controller = createTemporaryObject(controllerComponent, root)
            const panel = createTemporaryObject(panelComponent, root, { theme: appTheme, taskController: controller })
            const controls = findChild(panel, "sectionAxisControls")
            const slider = findChild(panel, "sectionAxisAngleSlider")
            verify(controls && slider)
            verify(!controls.visible)
            controller.usingSelectedPlane = true
            controller.usingAxis = true
            controller.planeLabel = "Axis on Bracket"
            verify(controls.visible)
            compare(findChild(panel, "sectionPlaneSource").text, "About: Axis on Bracket")
            // Across: no angle to set.
            verify(!slider.visible)
            controller.axisMode = "through"
            verify(slider.visible)
            controller.axisAngle = 45
            compare(slider.value, 45)
        }
}
}
