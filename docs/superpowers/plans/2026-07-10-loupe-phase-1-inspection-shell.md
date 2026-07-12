# Loupe Phase 1 Inspection Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the Phase 0 contracts into a responsive new Qt/QML shell with an isolated CAD worker, progressive assembly viewer, Inspect workspace, measurement, sectioning, unit review, and local cache.

**Architecture:** Keep `loupe-core` unchanged as the exact-CAD owner, host it in `loupe-worker`, and exchange versioned control messages over `QLocalSocket`. The shell maps immutable worker events into presentation models; Qt Quick 3D receives custom mesh buffers and instance tables through C++ renderer adapters.

**Tech Stack:** C++23, Qt 6.8+ LTS modules (Core, Gui, Qml, Quick, QuickControls2, Quick3D, Network, Test, QuickTest), OCCT, SQLite, CMake/Ninja, Qt Test, Qt Quick Test.

---

## Authoritative Vertical-Gate Execution (2026-07-12)

Phase 1 begins only after the Phase 0 backend evidence gate is committed. The browser prototype is the first Phase 1 task because it validates interaction contracts cheaply after backend correctness is proven; it is not an alternative runtime architecture.

### Gate A: Interaction and Process Boundary — Task 0 and Tasks 1–3

- Validate Inspect/Export interaction state with fixture JSON.
- Establish versioned protocol values, isolated worker lifecycle, cancellation, and shell controller state.
- Run focused tests per change, then one full Debug suite and one integrated gate review.
- Commit message: `feat: establish shell and worker contract`.

### Gate B: Inspect Data and Rendering — Tasks 4–7

- Stream the tree, present custom meshes/instances, implement selection/visibility/floating toolbar, and persist unit overrides.
- Gate tests cover repeated definitions, occurrence picking, progressive readiness, cancellation, and equivalent stable IDs/unit outcomes across platforms.
- Commit message: `feat: deliver Inspect workspace foundation`.

### Gate C: Inspection Tools and Reopen — Tasks 8–9

- Add measurement, sectioning, capture, cache, invalidation, and progressive reopen.
- Run full Debug and Release tests because rendering, cache formats, and optimized worker paths are involved.
- Commit message: `feat: complete inspection workflow`.

### Gate D: Dual-Platform Inspection Evidence — Task 10

- Once the M2 Pro has joined development, Windows 11 and macOS Apple Silicon are both mandatory for Debug, Release, interaction, performance, and corpus-backed inspection evidence.
- No platform may remain advisory or open.
- Commit message: `test: close Phase 1 dual-platform gate`.

### Validation Cadence

- Use focused test-first checks per change and one contract/quality review per vertical gate.
- Do not repeat full matrices after each QML/helper edit.
- Treat wrong geometry/units, worker crashes, privacy, nondeterminism, unbounded resources, required interaction failure, and platform divergence as blockers. Record lower-risk polish for Phase 3.

## Required Skill Preflight

- Invoke `frontend-design`, `web-design-guidelines`, `ui-ux-pro-max`, and `design-motion-principles` before Task 0.
- Invoke `qt-cmake-project` before Task 1.
- Invoke `qt-qml`, `qt-ui-design`, `frontend-design`, and `design-motion-principles` before Tasks 3–8.
- Invoke `qt-qml-test` before writing each QML test and `qt-qml-test-run` when executing it.
- Invoke `qt-qml-review` and `qt-cpp-review` at vertical-gate commit checkpoints rather than after every helper task.
- Invoke `qt-qml-profiler`, `ui-ux-pro-max`, `web-design-guidelines`, and `verification-before-completion` in Task 10.

## Approved Plan Corrections (2026-07-12)

- Task 0 first creates and commits `prototype/package-lock.json` with `npm install`; every later prototype verification uses `npm ci`. This makes the browser contract reproducible.
- Task 1 adds the Qt 6.8 module set to `vcpkg.json` before CMake configuration and preserves the existing P0 names `macos-arm64-debug` and `macos-arm64-release`; do not introduce a second macOS preset naming scheme. The current shared manifest baseline resolves Qt 6.11.1 (minimum supported API remains Qt 6.8); Windows and Apple Silicon must use the same manifest baseline.
- Task 2 treats server lifecycle as a protocol contract: a server name is per-user and per-launch, stale local endpoints are removed only after ownership/liveness validation, every request receives one terminal event, event queues are bounded, and cancellation acknowledgement is explicit.
- Tasks 8 and 9 use the P0 platform-adapter rule for offscreen capture and durable atomic cache replacement. No cache or capture code may embed Windows-only paths or APIs in the shared core.
- Task 0 uses a semantic component list with separate native selection buttons and export checkboxes, not an ARIA listbox. A listbox cannot validly contain independent checkbox controls; the export workflow requires both controls to remain keyboard-operable.
- Browser review feedback: put the two-workspace switcher in the top application bar; do not expose a command-search affordance until the native command registry supplies real commands and enablement reasons.
- Browser review feedback: Inspect starts with no component selected and a full-assembly viewport. The right inspector shows document properties in that state and switches automatically to component properties on selection; it has no redundant "Show properties" action.
- Browser review feedback: keep the inspection toolbar bottom-centered inside the viewport/canvas, not fixed to the application window. Use labeled Lucide outline icons, with text tooltips and accessible command names.
- Browser review feedback: replace the passive unit chip with a compact overall-bounds/effective-unit control that opens source-unit review. It must keep the current unit state visible and retain export blocking for unresolved units.
- Inspection metadata feedback: do not present a fabricated generic STEP "type" as a component property. When OCCT can supply the value, show source metadata with its source; otherwise show geometry-derived surface area and volume. Material assignment is local Loupe metadata on a unique definition, inherited by its occurrences, never written back to the STEP file. The inspector shows selected material, density, computed volume, and estimated mass; unknown/invalid geometry reports an explicit unavailable state rather than a guessed weight.
- Measurement feedback: activating Measure opens a task panel before picking. P1 modes are point-to-point distance, edge/curve length, planar surface-to-surface distance, radius/diameter, angle, surface area, volume, and selected bounds. The panel states the current mode, required pick count, unit, and clear/exit action.
- Section feedback: activating Section opens one visible section task panel. It supports one X/Y/Z plane, a selected planar-face normal when available, reverse/flip, numeric and direct position, cap, and a 2D-slice-only presentation toggle. Section is inspection-only and never mutates the export shape.
- Capture feedback: Capture opens properties before rendering. P1 captures PNG only, with transparent or selected solid background, 1x/2x/3x/4x scale presets plus a bounded custom multiplier, visible resolved dimensions, and inclusion toggles for measurements and section caps. JPEG/TIFF/vector capture remain out of scope for P1 so transparency and alpha behavior are never ambiguous.

## File and Module Map

```text
src/protocol/ProtocolTypes.*           Versioned commands/events and JSON codecs
src/worker/WorkerServer.*              QLocalServer lifecycle and request routing
src/worker/ImportSession.*             Cancellation and progressive import events
src/app/ApplicationController.*        Worker lifecycle, open-file, workspace state
src/app/models/AssemblyTreeModel.*      Immutable snapshot to QAbstractItemModel
src/app/models/SelectionModel.*         Viewer/tree transient selection
src/app/models/UnitReviewModel.*        Unit evidence and local override commands
src/app/render/MeshGeometry.*           QQuick3DGeometry buffer adapter
src/app/render/DefinitionInstances.*    QQuick3DInstancing adapter
src/app/render/SceneModel.*             Mesh, transform, visibility, ghost state
src/app/cache/CacheStore.*              SQLite metadata and versioned cache files
src/app/qml/Main.qml                    Shell and workspace switcher
src/app/qml/inspect/InspectWorkspace.qml
src/app/qml/inspect/InspectionDock.qml
src/app/qml/inspect/SectionControls.qml
src/app/qml/inspect/MeasurementOverlay.qml
src/app/qml/dialogs/UnitReviewDialog.qml
tests/protocol/                        Codec and compatibility tests
tests/worker/                          Worker process and cancellation tests
tests/app/                             Presentation-model and cache tests
tests/qml/                             Qt Quick Test interaction tests
```

## Task 0: Validate the Inspect/Export Interaction Contract

**Files:**
- Create: `prototype/package.json`
- Create: `prototype/package-lock.json`
- Create: `prototype/index.html`
- Create: `prototype/src/app.js`
- Create: `prototype/src/styles.css`
- Create: `prototype/src/fixture/assembly.json`
- Create: `prototype/tests/workspaces.spec.js`
- Create: `prototype/playwright.config.js`

- [ ] **Step 1: Write failing accessible interaction tests**

```javascript
test('Inspect is the default workspace with a floating tool surface', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByRole('tab', { name: 'Inspect' })).toHaveAttribute('aria-selected', 'true');
  for (const name of ['Fit', 'Isolate', 'Section', 'Measure', 'Ghost', 'Capture'])
    await expect(page.getByRole('button', { name })).toBeVisible();
});

test('highlight does not mutate the checked export set', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('tab', { name: 'Export' }).click();
  await page.getByRole('option', { name: /Front cover/ }).click();
  await expect(page.getByTestId('master-highlight')).toHaveText('Front cover');
  await expect(page.getByTestId('isolated-preview')).toHaveText('Front cover');
  await expect(page.getByRole('checkbox', { name: /Export Front cover/ })).not.toBeChecked();
});

test('unresolved units block export review', async ({ page }) => {
  await page.goto('/?fixture=suspicious-units');
  await page.getByRole('tab', { name: 'Export' }).click();
  await expect(page.getByRole('button', { name: 'Review export plan' })).toBeDisabled();
});
```

- [ ] **Step 2: Verify the tests fail before the prototype exists**

```powershell
Push-Location prototype
npm install
npx playwright install chromium
npm test
Pop-Location
```

Expected: `npm install` creates the committed lockfile, then tests FAIL because the application contract is absent. Every subsequent Task 0 run uses `npm ci`.

- [ ] **Step 3: Implement only the approved fixture-driven state contract**

Implement semantic workspace tabs, floating Inspect toolbar, component list with separate selection buttons and checkboxes, master context highlight/ghosting, isolated preview, and source-unit review. Keep highlight transient and checked export state persistent. Honor `prefers-reduced-motion` and visible keyboard focus.

- [ ] **Step 4: Run interaction, keyboard, and reduced-motion tests**

```powershell
Push-Location prototype
npm ci
npm test
Pop-Location
```

Expected: all interaction-contract tests pass. Review the prototype with representative fixture snapshots before translating it into QML.

- [ ] **Step 5: Commit the interaction contract**

```powershell
git add prototype
git commit -m "test: validate workspace interaction contract"
```

## Task 1: Add Qt Targets and Versioned Protocol Types

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `vcpkg.json`
- Create: `src/protocol/CMakeLists.txt`
- Create: `src/protocol/ProtocolTypes.h`
- Create: `src/protocol/ProtocolJson.cpp`
- Create: `src/worker/CMakeLists.txt`
- Create: `src/app/CMakeLists.txt`
- Create: `tests/protocol/test_protocol.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing protocol round-trip tests**

```cpp
#include <QtTest/QTest>
#include "protocol/ProtocolTypes.h"

class ProtocolTest final : public QObject {
    Q_OBJECT
private slots:
    void openFileRoundTrips() {
        const loupe::protocol::OpenFile command{42, R"(C:\fixtures\assembly.step)", std::nullopt};
        const QByteArray bytes = loupe::protocol::encode(command);
        const auto decoded = loupe::protocol::decodeCommand(bytes);
        QCOMPARE(std::get<loupe::protocol::OpenFile>(decoded).requestId, 42ULL);
    }
    void rejectsUnknownMajorVersion() {
        QVERIFY_EXCEPTION_THROWN(
            loupe::protocol::decodeEvent(R"({"version":{"major":99,"minor":0},"type":"ready"})"),
            loupe::protocol::ProtocolError);
    }
};
QTEST_MAIN(ProtocolTest)
#include "test_protocol.moc"
```

- [ ] **Step 2: Configure Qt and verify the test fails**

Add `qtbase`, `qtdeclarative`, `qtquick3d`, and `qttools` to the pinned vcpkg manifest, then configure with the existing P0 `windows-debug` preset. Keep all Qt packages at the manifest baseline shared by Windows and Apple Silicon.

Add:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Quick3D Network Test QuickTest Sql)
qt_standard_project_setup(REQUIRES 6.8)
add_subdirectory(src/protocol)
add_subdirectory(src/worker)
add_subdirectory(src/app)
```

Run `cmake --preset windows-debug` and build.

Expected: protocol test fails because the types and codecs are missing.

The P0 `macos-arm64-debug` and `macos-arm64-release` presets already exist. Verify they remain the only macOS preset names; do not add `macos-debug` or `macos-release` aliases.

- [ ] **Step 3: Define a closed, versioned command/event variant**

```cpp
// src/protocol/ProtocolTypes.h
#pragma once
#include <QByteArray>
#include <QString>
#include <cstdint>
#include <optional>
#include <variant>

namespace loupe::protocol {
struct Version { std::uint16_t major{1}; std::uint16_t minor{0}; };
struct UnitOverride { QString unit; double customFactor{1.0}; QString reason; };
struct OpenFile { std::uint64_t requestId{}; QString path; std::optional<UnitOverride> unitOverride; };
struct Cancel { std::uint64_t requestId{}; };
struct SetVisible { QString nodeId; bool visible{}; };
struct Ready { Version version; };
struct Progress { std::uint64_t requestId{}; QString stage; double fraction{}; };
struct SnapshotReady { std::uint64_t requestId{}; QByteArray snapshotJson; };
struct MeshReady { std::uint64_t requestId{}; QString definitionId; int refinement{}; QString segmentKey; };
struct Failed { std::uint64_t requestId{}; QString code; QString message; bool recoverable{}; };

using Command = std::variant<OpenFile, Cancel, SetVisible>;
using Event = std::variant<Ready, Progress, SnapshotReady, MeshReady, Failed>;

class ProtocolError final : public std::runtime_error { using std::runtime_error::runtime_error; };
QByteArray encode(const OpenFile& command);
Command decodeCommand(const QByteArray& bytes);
Event decodeEvent(const QByteArray& bytes);
}
```

Use newline-delimited compact JSON. Every frame contains `version`, `type`, and `requestId` where applicable. Reject unknown major versions; tolerate newer minor versions by ignoring unknown fields.

- [ ] **Step 4: Run protocol tests**

Run:

```powershell
cmake --build --preset windows-debug
ctest --preset windows-debug -R protocol --output-on-failure
```

Expected: command/event round trips and version rejection pass.

- [ ] **Step 5: Commit the protocol seam**

```powershell
git add CMakeLists.txt src/protocol src/worker/CMakeLists.txt src/app/CMakeLists.txt tests
git commit -m "feat: add versioned Loupe worker protocol"
```

## Task 2: Isolate the CAD Worker and Support Cancellation

**Files:**
- Create: `src/worker/main.cpp`
- Create: `src/worker/WorkerServer.h`
- Create: `src/worker/WorkerServer.cpp`
- Create: `src/worker/ImportSession.h`
- Create: `src/worker/ImportSession.cpp`
- Create: `tests/worker/test_worker_process.cpp`
- Modify: `src/worker/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing process-lifecycle tests**

```cpp
void WorkerProcessTest::readyThenCancel() {
    WorkerHarness worker;
    worker.start();
    QCOMPARE(worker.takeEventType(), QStringLiteral("ready"));
    worker.sendOpen(7, slowGeneratedFixture());
    worker.sendCancel(7);
    QCOMPARE(worker.takeTerminalEvent(7).type, QStringLiteral("canceled"));
    QVERIFY(worker.isRunning());
}

void WorkerProcessTest::invalidFileDoesNotCrashWorker() {
    WorkerHarness worker;
    worker.start();
    worker.sendOpen(8, QStringLiteral("missing.step"));
    QCOMPARE(worker.takeTerminalEvent(8).code, QStringLiteral("read_failed"));
    QVERIFY(worker.isRunning());
}
```

- [ ] **Step 2: Build and verify the tests fail**

Run `ctest --preset windows-debug -R worker-process --output-on-failure`.

Expected: worker executable or ready event is missing.

- [ ] **Step 3: Implement one active import session per worker**

```cpp
class ImportSession final : public QObject {
    Q_OBJECT
public:
    ImportSession(std::uint64_t requestId, QString path,
                  std::optional<protocol::UnitOverride> unitOverride,
                  QObject* parent = nullptr);
    void start(QThreadPool& pool);
    void cancel() noexcept { canceled_.store(true); }
signals:
    void progress(protocol::Progress event);
    void snapshot(protocol::SnapshotReady event);
    void mesh(protocol::MeshReady event);
    void failed(protocol::Failed event);
    void canceled(std::uint64_t requestId);
private:
    std::atomic_bool canceled_{false};
};
```

`WorkerServer` owns a `QLocalServer`, accepts one shell connection, writes `Ready`, dispatches commands, and keeps the process alive after per-request failures. `main.cpp` derives a per-user, per-launch unique server name from `--server-name` and exits nonzero only for server initialization failure. Before removing a stale local endpoint, the shell verifies that no live worker owns it. Each accepted request emits exactly one terminal event (`SnapshotReady`, `Failed`, or `Canceled`); `Cancel` produces an explicit terminal `Canceled` acknowledgement. Bound outbound progress/mesh queues by request and drop superseded non-terminal progress events, never terminal events.

- [ ] **Step 4: Verify cancellation and crash isolation**

Run:

```powershell
ctest --preset windows-debug -R worker-process --output-on-failure
```

Expected: cancellation produces a terminal canceled event, invalid files produce recoverable failures, and the worker remains available.

- [ ] **Step 5: Commit the worker boundary**

```powershell
git add src/worker tests/worker tests/CMakeLists.txt
git commit -m "feat: isolate CAD work in a cancellable process"
```

## Task 3: Build the Shell Controller and Workspace State

**Files:**
- Create: `src/app/main.cpp`
- Create: `src/app/ApplicationController.h`
- Create: `src/app/ApplicationController.cpp`
- Create: `src/app/qml/Main.qml`
- Create: `src/app/qml/Theme.qml`
- Create: `tests/app/test_application_controller.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing shell-state tests**

```cpp
void ApplicationControllerTest::inspectIsDefaultWorkspace() {
    ApplicationController controller(workerFactory());
    QCOMPARE(controller.workspace(), Workspace::Inspect);
}

void ApplicationControllerTest::workerCrashKeepsDocumentRecoverable() {
    ApplicationController controller(workerFactory());
    controller.openFile(validFixture());
    workerFactory().crashActiveWorker();
    QCOMPARE(controller.documentState(), DocumentState::WorkerFailed);
    QVERIFY(controller.canRetry());
}
```

- [ ] **Step 2: Build and verify missing controller failures**

Run `ctest --preset windows-debug -R application-controller --output-on-failure`.

Expected: compilation fails because `ApplicationController` is missing.

- [ ] **Step 3: Implement explicit workspace and document state**

```cpp
enum class Workspace { Inspect, Export };
enum class DocumentState { Empty, Opening, UnitReview, TreeReady, Viewing, WorkerFailed, Invalid };

class ApplicationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(Workspace workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged)
    Q_PROPERTY(DocumentState documentState READ documentState NOTIFY documentStateChanged)
    Q_PROPERTY(QString fileIdentity READ fileIdentity NOTIFY fileIdentityChanged)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY documentStateChanged)
public:
    Q_INVOKABLE void openFile(const QUrl& file);
    Q_INVOKABLE void cancelOpen();
    Q_INVOKABLE void retryWorker();
    Q_INVOKABLE void setWorkspace(Workspace workspace);
};
```

In `Main.qml`, use a semantic two-item workspace switcher, file identity with unit chip, command-search button, and a `StackLayout` that keeps both workspaces alive to preserve state.

- [ ] **Step 4: Add QML smoke loading and controller tests**

Run:

```powershell
ctest --preset windows-debug -R "application-controller|qml-smoke" --output-on-failure
```

Expected: Inspect is default, workspace state persists, worker failure is recoverable, and `Main.qml` loads without warnings.

- [ ] **Step 5: Commit the shell state**

```powershell
git add src/app tests/app tests/CMakeLists.txt
git commit -m "feat: add Loupe shell and workspace state"
```

## Task 4: Stream and Present the Assembly Tree

**Files:**
- Create: `src/app/models/AssemblyTreeModel.h`
- Create: `src/app/models/AssemblyTreeModel.cpp`
- Create: `src/app/models/SelectionModel.h`
- Create: `src/app/models/SelectionModel.cpp`
- Create: `src/app/qml/components/AssemblyTree.qml`
- Create: `tests/app/test_assembly_tree_model.cpp`
- Create: `tests/qml/tst_assembly_tree.qml`

- [ ] **Step 1: Write failing model and QML tests**

```cpp
void AssemblyTreeModelTest::repeatedDefinitionRoleIsExposed() {
    AssemblyTreeModel model;
    model.replaceSnapshot(repeatedFixtureSnapshot());
    const QModelIndex fastener = model.indexForStableId("occ-fastener-1");
    QCOMPARE(model.data(fastener, AssemblyTreeModel::DefinitionQuantityRole).toInt(), 12);
}
```

```qml
function test_search_and_focus() {
    searchField.text = "front cover"
    compare(tree.visibleRowCount, 1)
    tree.activateCurrent()
    compare(selectionModel.activeNodeId, "occ-front-cover")
}
```

- [ ] **Step 2: Run the focused tests and verify failure**

Run `ctest --preset windows-debug -R "assembly-tree|qml-tree" --output-on-failure`.

Expected: model roles and QML component are missing.

- [ ] **Step 3: Implement focused roles and reset-free updates**

Expose roles for stable ID, node kind, name, path, definition quantity, visibility, warning count, load stage, and selected state. Apply progressive snapshots by stable ID and emit targeted row changes; do not reset the model after the first tree becomes visible.

`SelectionModel` owns only transient active selection and emits `activeNodeChanged`. It contains no export check state.

- [ ] **Step 4: Verify search, keyboard focus, and repeated quantities**

Run:

```powershell
ctest --preset windows-debug -R "assembly-tree|qml-tree" --output-on-failure
```

Expected: search, keyboard activation, quantity roles, warnings, and selection synchronization pass.

- [ ] **Step 5: Commit the tree model**

```powershell
git add src/app/models src/app/qml/components tests
git commit -m "feat: present progressive assembly tree"
```

## Task 5: Render Custom Meshes and Repeated Instances

**Files:**
- Create: `src/app/render/MeshGeometry.h`
- Create: `src/app/render/MeshGeometry.cpp`
- Create: `src/app/render/DefinitionInstances.h`
- Create: `src/app/render/DefinitionInstances.cpp`
- Create: `src/app/render/SceneModel.h`
- Create: `src/app/render/SceneModel.cpp`
- Create: `src/app/qml/viewer/AssemblyView.qml`
- Create: `tests/app/test_scene_model.cpp`

- [ ] **Step 1: Write failing mesh/instance tests**

```cpp
void SceneModelTest::repeatedDefinitionSharesOneGeometry() {
    SceneModel scene;
    scene.applySnapshot(repeatedFixtureSnapshot());
    scene.applyMesh(coarseBoxMesh());
    QCOMPARE(scene.geometryCount(), 1);
    QCOMPARE(scene.instanceCount("def-box"), 2);
}

void SceneModelTest::selectionMapsPickInstanceToOccurrence() {
    SceneModel scene = repeatedScene();
    QCOMPARE(scene.nodeIdForPick("def-box", 1), QStringLiteral("occ-box-2"));
}
```

- [ ] **Step 2: Build and verify the render adapter is absent**

Run `ctest --preset windows-debug -R scene-model --output-on-failure`.

Expected: render types are missing.

- [ ] **Step 3: Implement Qt Quick 3D adapters**

`MeshGeometry` subclasses `QQuick3DGeometry`, accepts immutable interleaved position/normal data plus index data, sets bounds for picking, and calls `update()` only on refinement replacement.

`DefinitionInstances` subclasses `QQuick3DInstancing`, stores one row per occurrence, and encodes transform, base color, selection/ghost custom data, and stable instance order.

`SceneModel` maps definition IDs to one geometry and one instance table, and maps pick `(definitionId, instanceIndex)` back to occurrence ID.

- [ ] **Step 4: Verify coarse replacement and picking**

Run:

```powershell
ctest --preset windows-debug -R scene-model --output-on-failure
```

Expected: repeated definitions share geometry, refinement replaces buffers without changing stable instance order, and picks resolve to occurrences.

- [ ] **Step 5: Commit the viewer adapter**

```powershell
git add src/app/render src/app/qml/viewer tests/app
git commit -m "feat: render streamed assembly instances"
```

## Task 6: Implement Inspect Selection, Visibility, and Floating Toolbar

**Files:**
- Create: `src/app/qml/inspect/InspectWorkspace.qml`
- Create: `src/app/qml/inspect/InspectionDock.qml`
- Create: `src/app/qml/inspect/ContextPanel.qml`
- Create: `src/app/models/VisibilityModel.h`
- Create: `src/app/models/VisibilityModel.cpp`
- Create: `tests/qml/tst_inspect_workspace.qml`
- Create: `tests/app/test_visibility_model.cpp`

- [ ] **Step 1: Write failing interaction tests**

```qml
function test_dock_has_stable_tools() {
    compare(dock.toolNames, ["Fit", "Isolate", "Section", "Measure", "Ghost", "Capture"])
    const before = dock.mapToItem(workspace, 0, 0)
    selectionModel.activeNodeId = "occ-front-cover"
    const after = dock.mapToItem(workspace, 0, 0)
    compare(after.x, before.x)
    compare(after.y, before.y)
}

function test_tree_and_view_share_selection() {
    tree.activateNode("occ-front-cover")
    compare(viewer.highlightedNodeId, "occ-front-cover")
    viewer.simulatePick("occ-fastener-4")
    compare(tree.currentNodeId, "occ-fastener-4")
}
```

- [ ] **Step 2: Run tests and verify the workspace is missing**

Run `ctest --preset windows-debug -R inspect-workspace --output-on-failure`.

Expected: QML component and visibility model are missing.

- [ ] **Step 3: Implement visibility snapshots and stable dock actions**

`VisibilityModel` supports `hide`, `show`, `isolate`, `ghostComplements`, and `restorePrevious`, keyed by stable node IDs. Isolate saves the complete previous visibility/ghost state as one undoable presentation snapshot.

`InspectionDock.qml` stays bottom-centered, provides 44 px minimum hit targets, text tooltips, keyboard shortcuts, and disabled reasons. It must not contain export checkboxes.

- [ ] **Step 4: Verify selection, isolate restore, focus, and reduced motion**

Run:

```powershell
ctest --preset windows-debug -R "inspect-workspace|visibility-model" --output-on-failure
```

Expected: tree/view synchronization, isolate/restore, dock stability, keyboard focus, and reduced-motion variants pass.

- [ ] **Step 5: Commit the Inspect workspace**

```powershell
git add src/app/qml/inspect src/app/models tests
git commit -m "feat: add Inspect workspace controls"
```

## Task 7: Add Source Unit Review and Override Persistence

**Files:**
- Create: `src/app/models/UnitReviewModel.h`
- Create: `src/app/models/UnitReviewModel.cpp`
- Create: `src/app/qml/dialogs/UnitReviewDialog.qml`
- Create: `src/app/cache/OverrideStore.h`
- Create: `src/app/cache/OverrideStore.cpp`
- Create: `tests/app/test_override_store.cpp`
- Create: `tests/qml/tst_unit_review.qml`

- [ ] **Step 1: Write failing override and UI tests**

```cpp
void OverrideStoreTest::sourceChangeInvalidatesOverride() {
    OverrideStore store(tempDatabase());
    store.put(SourceIdentity{"hash-a", 100, 10}, UnitOverride{"in", 1.0, "reviewed"});
    QVERIFY(store.get(SourceIdentity{"hash-a", 100, 10}).has_value());
    QVERIFY_FALSE(store.get(SourceIdentity{"hash-b", 101, 11}).has_value());
}
```

```qml
function test_preview_before_apply() {
    model.declaredUnit = "mm"
    model.normalizedExtents = Qt.vector3d(4, 2, 1)
    dialog.open()
    unitSelect.currentValue = "in"
    verify(correctedPreview.text.includes("101.6"))
    compare(model.effectiveUnit, "mm")
    applyButton.clicked()
    compare(model.effectiveUnit, "in")
}
```

- [ ] **Step 2: Run tests and verify missing model/store failures**

Run `ctest --preset windows-debug -R "override-store|unit-review" --output-on-failure`.

Expected: missing types/components fail compilation or QML loading.

- [ ] **Step 3: Implement explicit preview/apply behavior**

`UnitReviewModel` exposes declared units, XCAF unit, current extents, confidence, reason, proposed unit, proposed custom factor, corrected extents, and `applyOverride(reason)`. Changing a proposal never changes the scene until Apply.

`OverrideStore` uses SQLite table:

```sql
CREATE TABLE unit_override (
  source_hash TEXT NOT NULL,
  source_size INTEGER NOT NULL,
  source_mtime INTEGER NOT NULL,
  unit TEXT NOT NULL,
  custom_factor REAL NOT NULL,
  reason TEXT NOT NULL,
  PRIMARY KEY(source_hash, source_size, source_mtime)
);
```

- [ ] **Step 4: Verify invalidation, export blocking, and accessible warnings**

Run:

```powershell
ctest --preset windows-debug -R "override-store|unit-review" --output-on-failure
```

Expected: corrected previews, source-change invalidation, explicit Apply, warning announcements, and unresolved-unit blocking pass.

- [ ] **Step 5: Commit unit review**

```powershell
git add src/app/models/UnitReviewModel.* src/app/qml/dialogs src/app/cache tests
git commit -m "feat: review and persist source unit overrides"
```

## Task 8: Add Measurement, Sectioning, and Capture

**Files:**
- Create: `src/app/tools/MeasurementController.*`
- Create: `src/app/tools/SectionController.*`
- Create: `src/app/tools/CaptureController.*`
- Create: `src/app/qml/inspect/MeasurementOverlay.qml`
- Create: `src/app/qml/inspect/SectionControls.qml`
- Create: `tests/app/test_inspection_tools.cpp`
- Create: `tests/qml/tst_inspection_tools.qml`

- [ ] **Step 1: Write failing tool tests**

```cpp
void InspectionToolsTest::distanceUsesEffectiveUnits() {
    MeasurementController controller;
    controller.setEffectiveUnit(Unit::Inch);
    const auto measurement = controller.pointDistance({0, 0, 0}, {25.4, 0, 0});
    QCOMPARE(measurement.displayValue, 1.0);
    QCOMPARE(measurement.unitLabel, QStringLiteral("in"));
}

void InspectionToolsTest::sectionDoesNotAffectExportShape() {
    SectionController section;
    section.setEnabled(true);
    section.setAxis(Axis::X);
    QCOMPARE(section.exportMutationCount(), 0);
}
```

- [ ] **Step 2: Run tests and verify tools are missing**

Run `ctest --preset windows-debug -R inspection-tools --output-on-failure`.

Expected: missing controllers fail compilation.

- [ ] **Step 3: Implement inspection-only controllers**

Measurement supports point distance, edge/face length, radius/diameter, angle, and selected bounds. Section supports one X/Y/Z plane, reverse, numeric/interactive position, and cap. Capture renders the current viewer, measurement overlays, and section state to transparent PNG at 2x, 3x, or 4x without resizing the live window. Keep offscreen render-target creation and file finalization behind platform adapters so Windows and macOS use equivalent shared controller semantics.

- [ ] **Step 4: Verify QML controls and output dimensions**

Run:

```powershell
ctest --preset windows-debug -R inspection-tools --output-on-failure
```

Expected: measurement units, section controls, non-destructive behavior, focus handling, and 2x-4x transparent capture dimensions pass.

- [ ] **Step 5: Commit inspection tools**

```powershell
git add src/app/tools src/app/qml/inspect tests
git commit -m "feat: add measurement section and capture tools"
```

## Task 9: Add Versioned Local Cache and Progressive Reopen

**Files:**
- Create: `src/app/cache/CacheStore.h`
- Create: `src/app/cache/CacheStore.cpp`
- Create: `src/app/cache/CacheKey.h`
- Create: `tests/app/test_cache_store.cpp`

- [ ] **Step 1: Write failing cache tests**

```cpp
void CacheStoreTest::effectiveUnitChangesKey() {
    const auto mm = CacheKey::from(source(), "importer-1", "coarse", UnitDecision{"mm", 1.0});
    const auto in = CacheKey::from(source(), "importer-1", "coarse", UnitDecision{"in", 25.4});
    QVERIFY(mm != in);
}

void CacheStoreTest::evictsLeastRecentlyUsedOverBudget() {
    CacheStore store(tempDirectory(), 1024);
    store.put(key("a"), bytes(700));
    store.put(key("b"), bytes(700));
    QVERIFY_FALSE(store.contains(key("a")));
    QVERIFY(store.contains(key("b")));
}
```

- [ ] **Step 2: Run tests and verify cache types are missing**

Run `ctest --preset windows-debug -R cache-store --output-on-failure`.

Expected: compilation fails.

- [ ] **Step 3: Implement local-only cache metadata and atomic files**

Cache metadata fields are source hash, size, mtime, importer version, mesh profile, effective unit/factor, schema version, last access, byte size, snapshot path, and mesh path. Resolve the root through `QStandardPaths::AppLocalDataLocation`; reject UNC/network roots and synced overrides. Write cache files through a platform adapter to temporary names, durably close them, then atomically replace the final path; the shared cache layer must not call Windows-only APIs.

- [ ] **Step 4: Verify invalidation, LRU, cancellation, and clear-cache**

Run:

```powershell
ctest --preset windows-debug -R cache-store --output-on-failure
```

Expected: source/importer/unit changes invalidate, canceled writes leave no entry, LRU respects budget, and Clear cache removes only Loupe cache files.

- [ ] **Step 5: Commit cache support**

```powershell
git add src/app/cache tests/app
git commit -m "feat: add versioned local assembly cache"
```

## Task 10: Verify Phase 1 UX, Performance, and Platform Gate

**Files:**
- Create: `tests/qml/tst_keyboard_accessibility.qml`
- Create: `tools/bench/inspect_benchmark.cpp`
- Create: `docs/evidence/phase-1-template.md`
- Modify: `docs/evidence/README.md`

- [ ] **Step 1: Add accessibility and reduced-motion tests**

```qml
function test_focus_order_matches_visual_order() {
    compare(tabOrder(), ["workspaceSwitch", "commandSearch", "treeSearch", "assemblyTree", "inspectionDock"])
}

function test_reduced_motion_preserves_selection() {
    Theme.reducedMotion = true
    selectionModel.activeNodeId = "occ-front-cover"
    compare(viewer.highlightedNodeId, "occ-front-cover")
    compare(workspace.transitionDuration, 0)
}
```

- [ ] **Step 2: Add benchmark measurements with machine-readable output**

`inspect_benchmark` records shell-ready, file acknowledgement, tree-ready, coarse-view, first-interaction, cached-reopen, selection latency p50/p95, frame time p50/p95, peak memory, and idle CPU. Output one JSON object plus CSV rows keyed by source hash.

- [ ] **Step 3: Run all native, QML, and benchmark verification**

Run:

```powershell
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
build/windows-release/tools/bench/inspect_benchmark.exe corpus/private/cases.json --out evidence/phase-1/windows
```

Expected: all tests pass; benchmark files contain every required metric for every selected reference case.

- [ ] **Step 4: Record the explicit platform gate**

The Phase 1 report must state:

```text
Windows 11 x64: measured pass/fail per target.
Apple Silicon macOS: open until built, signed for local test, and benchmarked on real M-series hardware.
No cross-platform completion claim is allowed while the macOS row is open.
```

- [ ] **Step 5: Commit the Phase 1 evidence checkpoint**

```powershell
git add tests/qml tools/bench docs/evidence
git commit -m "test: establish Phase 1 inspection gate"
```

## Legacy Phase 1 Completion Gate (Superseded)

- Worker failures and cancellation do not take down the shell.
- Inspect is the default workspace and preserves state.
- Progressive tree/coarse meshes appear before full refinement.
- Repeated definitions use shared geometry and pick correct occurrences.
- Measure, section, isolate, ghost, capture, and unit review pass automated tests.
- Cache invalidation includes effective unit interpretation.
- Windows reference measurements are recorded.
- The macOS gate is closed only after real Apple Silicon verification.

## Phase 1 Completion Gate (Authoritative)

- Phase 0 backend evidence is committed before Task 0 begins.
- The browser contract is reviewed before QML implementation.
- Worker failures and cancellation do not take down the shell.
- Inspect is the default workspace and preserves state.
- Progressive tree/coarse meshes appear before full refinement.
- Repeated definitions use shared geometry and pick correct occurrences.
- Measure, section, isolate, ghost, capture, and unit review pass automated tests.
- Cache invalidation includes effective unit interpretation.
- Windows 11 and macOS Apple Silicon both pass Debug, Release, interaction, corpus-backed inspection, and performance gates. No advisory platform row is allowed after macOS development begins.
- Gate evidence is committed and the worktree is clean.
