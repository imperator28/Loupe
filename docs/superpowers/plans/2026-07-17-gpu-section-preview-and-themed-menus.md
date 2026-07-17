# GPU Section Preview and Themed Menus Implementation Plan

## Objective

Replace pointer-driven CPU section reconstruction with GPU clipping, preserve exact capped and 2D output after release, correct the removed-side arrow convention, improve fallback-slider acquisition, and theme every popup menu explicitly.

## Task 1: Lock interaction behavior with tests

Files:

- Modify `tests/app/test_inspection_tools.cpp`
- Modify `tests/qml/tst_SectionControls.qml`
- Add `tests/qml/tst_ThemedMenus.qml`

Work:

- Assert begin/preview/commit/cancel produces one exact commit and no intermediate exact requests.
- Assert the removed-side direction is `-normal` when not flipped and `+normal` when flipped.
- Assert slider dragging is relative, bounded, and one-tenth sensitivity with Shift.
- Assert Dark and Light menu items expose explicit readable text and highlighted-text colors.

Verification:

```sh
ctest --test-dir build/macos-arm64-debug --output-on-failure -R 'inspection-tools|qml-inspection-tools'
```

## Task 2: Add reusable themed menu controls

Files:

- Add `src/app/qml/inspect/ThemedMenu.qml`
- Add `src/app/qml/inspect/ThemedMenuItem.qml`
- Add `src/app/qml/inspect/ThemedMenuSeparator.qml`
- Modify `src/app/CMakeLists.txt`
- Modify `src/app/qml/inspect/InspectWorkspace.qml`
- Modify `src/app/qml/inspect/StepViewport.qml`

Work:

- Use Basic-style control customization only inside the reusable components.
- Bind popup background, border, normal text, disabled text, highlighted text, check state, and separator colors to the active Loupe theme.
- Replace assembly-tree, viewport, component, and display-style native menu delegates.

Verification:

- Run the themed-menu QML test.
- Open every context menu in Dark and Light themes during the live smoke test.

## Task 3: Correct arrow and slider interaction

Files:

- Modify `src/app/qml/inspect/StepViewport.qml`
- Modify `src/app/qml/inspect/SectionManipulator.qml`
- Modify `src/app/qml/inspect/SectionOffsetSlider.qml`
- Modify `tests/qml/tst_SectionControls.qml`

Work:

- Introduce an explicit removed-half-space normal: flipped uses the effective normal; unflipped uses its inverse.
- Drive the 3D arrow, projected hit target, and drag axis from that removed-side normal.
- Keep the committed section offset unchanged when Flip changes.
- Expand the slider rail to 64 pixels and the handle to 30 by 18 pixels.
- Make every slider press start a relative drag without jumping; retain Shift fine adjustment and keyboard control.

## Task 4: Add the GPU clipping material

Files:

- Add `src/app/qml/inspect/SectionClipMaterial.qml`
- Add `src/app/qml/inspect/shaders/section-clip.frag`
- Modify `src/app/CMakeLists.txt`
- Modify `src/app/qml/inspect/StepViewport.qml`

Work:

- Implement a shaded `CustomMaterial` using `VAR_WORLD_POSITION`.
- Expose base color, roughness, clip enabled, plane normal, plane offset, and retained-side sign as QML uniforms.
- Discard fragments whose signed plane distance is in the removed half-space.
- Use the same fragment clip for solid and CAD-line materials; preserve the configured line width.
- Keep PrincipledMaterial active when sectioning is disabled.

Reference:

- Qt `CustomMaterial`: https://doc.qt.io/qt-6/qml-qtquick3d-custommaterial.html
- Qt programmable material built-ins: https://doc.qt.io/qt-6/qtquick3d-custom.html

## Task 5: Keep source geometry immutable and separate exact overlays

Files:

- Modify `src/app/render/MeshGeometry.h`
- Modify `src/app/render/MeshGeometry.cpp`
- Modify `src/app/render/CadEdgeGeometry.h`
- Modify `src/app/render/CadEdgeGeometry.cpp`
- Modify `src/app/qml/inspect/StepViewport.qml`
- Modify `tests/app/test_scene_model.cpp`

Work:

- Stop rebuilding source solid and CAD-edge buffers for ordinary section updates.
- Add an overlay-only section mode that generates caps, 2D fill, and 2D outline without reproducing clipped body surfaces.
- Create one section overlay model per mesh segment and keep it hidden during drag.
- During drag, show GPU-clipped source solids and edges regardless of cap or 2D mode.
- After release, show exact cap overlays for ordinary section mode or exact fill/outline overlays for 2D mode.

## Task 6: Finalize exact overlays asynchronously

Files:

- Add `src/app/render/SectionMeshBuilder.h`
- Add `src/app/render/SectionMeshBuilder.cpp`
- Modify `src/app/render/MeshGeometry.h`
- Modify `src/app/render/MeshGeometry.cpp`
- Modify `src/app/CMakeLists.txt`
- Modify `src/app/qml/inspect/StepViewport.qml`
- Modify `tests/app/test_scene_model.cpp`

Work:

- Extract exact cap and 2D contour generation into a pure builder operating on immutable copied/shared source arrays.
- Run overlay builds through `QtConcurrent` with a monotonically increasing request generation.
- Apply completed buffers only on the GUI thread and only when their generation matches the latest committed request.
- Invalidate pending exact work when a new drag starts, a document closes, or sectioning is disabled.
- Expose per-overlay busy state so the viewport can show compact `Finalizing section` progress without blocking navigation.

## Task 7: Integrate state transitions and fallback behavior

Files:

- Modify `src/app/qml/inspect/StepViewport.qml`
- Modify `src/app/tools/SectionController.h`
- Modify `src/app/tools/SectionController.cpp`
- Modify `tests/app/test_inspection_tools.cpp`

Work:

- Make preview state update only shader uniforms and manipulator position.
- Commit one exact overlay generation on release.
- Restore the interaction start offset on cancel without displaying stale overlays.
- Keep the prior exact overlay visible until the replacement is ready, except in 2D-only mode where GPU-clipped solids serve as the drag preview.
- If the clipping material cannot initialize, keep the manipulator responsive and use plane-only preview plus exact-on-release; never restore per-pointer CPU rebuilding.

## Task 8: Build and review

Commands:

```sh
cmake --build build/macos-arm64-debug --parallel 8
ctest --test-dir build/macos-arm64-debug --output-on-failure -R 'application-controller|qml-smoke|qml-inspection-tools|scene-model|inspection-tools'
git diff --check
```

Live review:

- Launch `build/macos-arm64-debug/src/app/Loupe.app`.
- Load the largest practical STEP corpus assembly.
- Verify Dark and Light popup readability.
- Verify arrow direction and Flip behavior.
- Drag the scene handle and fallback slider continuously; confirm the cut follows the pointer and no stale positions replay.
- Release with caps enabled and in each 2D display mode; confirm exact output replaces the preview once and the UI stays responsive.
- Record preview responsiveness and exact-finalization duration separately.
