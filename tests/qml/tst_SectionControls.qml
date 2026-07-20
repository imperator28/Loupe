import QtQuick
import QtQuick3D
import QtTest
import "../../src/app/qml/inspect"

Item {
    id: root
    width: 640
    height: 480

    QtObject {
        id: mockTheme
        property color surfaceRaised: "#182027"
        property color border: "#34414b"
        property color accent: "#67d5c0"
    }

    Component {
        id: manipulatorComponent
        SectionManipulator {}
    }

    Component {
        id: sliderComponent
        SectionOffsetSlider {
            theme: mockTheme
            from: -20
            to: 20
            value: 0
        }
    }

    TestCase {
        name: "SectionControlsTests"
        when: windowShown

        function test_manipulatorAlignsWithCardinalNormals() {
            const manipulator = createTemporaryObject(manipulatorComponent, root)
            verify(!!manipulator)

            const up = manipulator.rotationForNormal(Qt.vector3d(0, 1, 0))
            verify(Math.abs(up.scalar - 1) < 0.00001)

            const down = manipulator.rotationForNormal(Qt.vector3d(0, -1, 0))
            verify(Math.abs(down.scalar) < 0.00001)
            verify(Math.abs(Math.abs(down.x) - 1) < 0.00001)

            const right = manipulator.rotationForNormal(Qt.vector3d(1, 0, 0))
            verify(Math.abs(right.scalar - Math.SQRT1_2) < 0.00001)
            verify(Math.abs(Math.abs(right.z) - Math.SQRT1_2) < 0.00001)
        }

        function test_manipulatorPointsTowardRemovedSide() {
            const manipulator = createTemporaryObject(manipulatorComponent, root)
            verify(!!manipulator)

            const retainedPositive = manipulator.removedSideNormal(Qt.vector3d(0, 0, 1), false)
            compare(retainedPositive.x, 0)
            compare(retainedPositive.y, 0)
            compare(retainedPositive.z, -1)

            const retainedNegative = manipulator.removedSideNormal(Qt.vector3d(0, 0, 1), true)
            compare(retainedNegative.x, 0)
            compare(retainedNegative.y, 0)
            compare(retainedNegative.z, 1)
        }

        function test_sliderBoundsKeyboardStyleEdits() {
            const slider = createTemporaryObject(sliderComponent, root)
            verify(!!slider)
            const spy = signalSpy.createObject(root, { target: slider, signalName: "valueEdited" })

            compare(slider.boundedValue(-25), -20)
            compare(slider.boundedValue(25), 20)
            slider.moveBy(1, false)

            compare(spy.count, 1)
            verify(spy.signalArguments[0][0] > 0)
        }


        function test_sliderUsesBoundedRelativeDrag() {
            const slider = createTemporaryObject(sliderComponent, root)
            verify(!!slider)

            const normal = slider.valueForDrag(5, 40, false)
            const fine = slider.valueForDrag(5, 40, true)
            verify(normal < 5)
            verify(fine < 5)
            verify(Math.abs((5 - fine) * 10 - (5 - normal)) < 0.00001)
            compare(slider.valueForDrag(19, -10000, false), 20)
            compare(slider.valueForDrag(-19, 10000, false), -20)
        }
    }

    Component { id: signalSpy; SignalSpy {} }
}
