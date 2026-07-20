import QtQuick
import QtQuick3D
import QtTest
import "../../src/app/qml/inspect"

Item {
    id: root
    width: 800
    height: 600

    QtObject {
        id: cameraRig
        property vector3d position: Qt.vector3d(0, 0, 0)
        property quaternion rotation: Quaternion.fromEulerAngles(0, 0, 0)
    }

    QtObject {
        id: perspectiveCamera
        property vector3d position: Qt.vector3d(0, 0, 0)
        property real fieldOfView: 45
    }

    QtObject {
        id: orthographicCamera
        property vector3d position: Qt.vector3d(0, 0, 0)
        property real horizontalMagnification: 1
        property real verticalMagnification: 1
    }

    Component {
        id: navigationComponent
        ViewportNavigation {
            cameraRig: cameraRig
            perspectiveCamera: perspectiveCamera
            orthographicCamera: orthographicCamera
            viewportWidth: root.width
            viewportHeight: root.height
        }
    }

    TestCase {
        name: "ViewportNavigationTests"
        when: windowShown

        function fuzzyVector(actual, expected, epsilon) {
            return Math.abs(actual.x - expected.x) <= epsilon
                    && Math.abs(actual.y - expected.y) <= epsilon
                    && Math.abs(actual.z - expected.z) <= epsilon
        }

        function fuzzyQuaternion(actual, expected, epsilon) {
            return Math.abs(actual.scalar - expected.scalar) <= epsilon
                    && Math.abs(actual.x - expected.x) <= epsilon
                    && Math.abs(actual.y - expected.y) <= epsilon
                    && Math.abs(actual.z - expected.z) <= epsilon
        }

        function test_resetUsesOrthographicIsometricView() {
            const navigation = createTemporaryObject(navigationComponent, root)
            verify(!!navigation)
            navigation.reset()

            compare(navigation.projectionMode, "orthographic")
            verify(Math.abs(navigation.orientation.x) > 0.1)
            verify(Math.abs(navigation.orientation.y) > 0.1)
        }

        function test_pendingPivotDoesNotMoveOrReorientCamera() {
            const navigation = createTemporaryObject(navigationComponent, root)
            navigation.reset()
            const beforePosition = navigation.cameraPosition
            const beforeOrientation = navigation.orientation

            navigation.setPendingOrbitPivot(Qt.vector3d(12, 8, 3))
            navigation.activatePendingOrbitPivot()

            verify(fuzzyVector(navigation.cameraPosition, beforePosition, 0.00001))
            verify(fuzzyQuaternion(navigation.orientation, beforeOrientation, 0.00001))
            verify(fuzzyVector(navigation.activePivot, Qt.vector3d(12, 8, 3), 0.00001))
        }

        function test_orbitKeepsPivotDistanceStable() {
            const navigation = createTemporaryObject(navigationComponent, root)
            navigation.reset()
            navigation.setPendingOrbitPivot(Qt.vector3d(10, 5, 0))
            navigation.activatePendingOrbitPivot()
            const beforeDistance = navigation.vectorLength(navigation.cameraOffset())

            navigation.orbit(24, -11)

            const afterDistance = navigation.vectorLength(navigation.cameraOffset())
            verify(Math.abs(afterDistance - beforeDistance) < 0.001)
        }

        function test_projectionRoundTripPreservesOrientationAndScale() {
            const navigation = createTemporaryObject(navigationComponent, root)
            navigation.fitBounds(Qt.vector3d(-20, -10, -5), Qt.vector3d(20, 10, 5))
            const beforeOrientation = navigation.orientation
            const beforeMagnification = navigation.orthographicMagnification

            navigation.setProjection("perspective")
            compare(navigation.projectionMode, "perspective")
            navigation.setProjection("orthographic")

            compare(navigation.projectionMode, "orthographic")
            verify(fuzzyQuaternion(navigation.orientation, beforeOrientation, 0.00001))
            verify(Math.abs(navigation.orthographicMagnification - beforeMagnification) < 0.001)
        }

        function test_panMovesCameraAndPivotTogether() {
            const navigation = createTemporaryObject(navigationComponent, root)
            navigation.reset()
            const beforeOffset = navigation.cameraOffset()

            navigation.pan(18, -7)

            verify(fuzzyVector(navigation.cameraOffset(), beforeOffset, 0.0001))
        }

        function test_alignToNormalAimsCameraAtPivot() {
            const navigation = createTemporaryObject(navigationComponent, root)
            navigation.reset()

            navigation.alignToNormal(Qt.vector3d(0, 0, 1))

            verify(fuzzyVector(navigation.cameraPosition, Qt.vector3d(0, 0, 120), 0.0001))
            verify(fuzzyVector(navigation.orientation.times(Qt.vector3d(0, 0, -1)),
                               Qt.vector3d(0, 0, -1), 0.0001))
        }
    }
}
