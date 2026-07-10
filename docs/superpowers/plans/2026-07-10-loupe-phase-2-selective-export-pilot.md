# Loupe Phase 2 Selective Export Pilot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the dedicated Export workspace and a complete reviewed, unit-resolved, validated STEP/STL export workflow suitable for internal pilot distribution.

**Architecture:** Extend the Phase 1 shell with a lazily loaded export module that owns persistent export check state, dual-view presentation, naming/grouping, and immutable plan review. Execute plans only in the CAD worker, validate every output independently, and return manifest-ready results to the shell.

**Tech Stack:** C++23, Qt 6 Quick/QML/Quick3D, OCCT XDE, SQLite, nlohmann/json, Qt Test, Qt Quick Test, CPack/WiX or MSIX packaging, macOS codesign/notarytool.

---

## Required Skill Preflight

- Invoke `qt-qml`, `qt-ui-design`, `frontend-design`, and `design-motion-principles` before Tasks 1–5.
- Invoke `qt-qml-test` and `qt-qml-test-run` for every QML task.
- Invoke `test-driven-development` before implementation, then `qt-qml-review` and `qt-cpp-review` before each commit.
- Invoke `ui-ux-pro-max`, `web-design-guidelines`, `qt-qml-profiler`, and `verification-before-completion` before the pilot gate.

## File and Module Map

```text
src/export/ExportSelectionModel.*      Checkbox state independent from highlight
src/export/ComponentListModel.*        Friendly list/group/filter presentation
src/export/NamingRules.*               Prefix/suffix/search/replace/sequence/CJK
src/export/PlanBuilder.*               UI choices to immutable core ExportPlan
src/export/ManifestWriter.*            JSON and CSV traceability
src/app/qml/export/ExportWorkspace.qml Three-column dual-view workspace
src/app/qml/export/ComponentList.qml    Search, filters, semantic checkboxes
src/app/qml/export/ExportMasterView.qml Assembly context and ghost state
src/app/qml/export/IsolatedPartView.qml Focused component confirmation
src/app/qml/export/PlanReview.qml       Per-output review and blocking issues
src/app/qml/export/ExportResults.qml    Per-row validated/warned/failed results
src/worker/ExportSession.*              Worker execution, progress, cancellation
src/core/export/*                       Extended Phase 0 writers and policies
src/core/validation/*                   Mandatory per-output read-back
packaging/windows/                      Pilot installer configuration
packaging/macos/                        Bundle, signing, notarization scripts
tests/export/                           Plan, naming, manifest, worker tests
tests/qml/                              Export workspace interaction tests
```

## Task 1: Separate Export Check State from Viewer Highlight

**Files:**
- Create: `src/export/CMakeLists.txt`
- Create: `src/export/ExportSelectionModel.h`
- Create: `src/export/ExportSelectionModel.cpp`
- Create: `tests/export/test_export_selection_model.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing selection-state tests**

```cpp
void ExportSelectionModelTest::highlightNeverMutatesCheckedRows() {
    ExportSelectionModel model;
    model.setChecked("def-fastener", SelectionKind::Definition, true);
    model.setHighlightedNodeId("occ-front-cover");
    QCOMPARE(model.checkedCount(), 1);
    QVERIFY(model.isChecked("def-fastener"));
    QVERIFY_FALSE(model.isChecked("occ-front-cover"));
}

void ExportSelectionModelTest::definitionAndOccurrenceCannotDuplicateOutputIntent() {
    ExportSelectionModel model;
    model.setChecked("def-cover", SelectionKind::Definition, true);
    const auto result = model.setChecked("occ-cover-2", SelectionKind::Occurrence, true);
    QCOMPARE(result, CheckResult::ConflictRequiresChoice);
}
```

- [ ] **Step 2: Build and verify tests fail**

Run `ctest --preset windows-debug -R export-selection --output-on-failure`.

Expected: export selection model is missing.

- [ ] **Step 3: Implement explicit roles and conflict results**

```cpp
enum class CheckResult { Applied, Removed, ConflictRequiresChoice, UnknownNode };

class ExportSelectionModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString highlightedNodeId READ highlightedNodeId WRITE setHighlightedNodeId NOTIFY highlightedNodeIdChanged)
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedCountChanged)
public:
    enum Role { NodeIdRole = Qt::UserRole + 1, KindRole, CheckedRole, ConflictRole };
    Q_INVOKABLE CheckResult setChecked(const QString& nodeId, SelectionKind kind, bool checked);
    Q_INVOKABLE bool isChecked(const QString& nodeId) const;
    Q_INVOKABLE void clearChecked();
};
```

Highlight is a separate property and must not appear in the serialized export selection list.

- [ ] **Step 4: Verify selection persistence and conflict recovery**

Run `ctest --preset windows-debug -R export-selection --output-on-failure`.

Expected: highlight separation, checked counts, definition/occurrence conflicts, clear, and serialization tests pass.

- [ ] **Step 5: Commit export selection state**

```powershell
git add CMakeLists.txt src/export tests/export tests/CMakeLists.txt
git commit -m "feat: add persistent export selection model"
```

## Task 2: Build the Friendly Component List

**Files:**
- Create: `src/export/ComponentListModel.h`
- Create: `src/export/ComponentListModel.cpp`
- Create: `src/app/qml/export/ComponentList.qml`
- Create: `tests/export/test_component_list_model.cpp`
- Create: `tests/qml/tst_export_component_list.qml`

- [ ] **Step 1: Write failing grouping and keyboard tests**

```cpp
void ComponentListModelTest::groupsRepeatedDefinitionsWithQuantity() {
    ComponentListModel model(repeatedAssemblySnapshot());
    const QModelIndex row = model.indexForDefinition("def-fastener");
    QCOMPARE(model.data(row, ComponentListModel::QuantityRole).toInt(), 12);
    QCOMPARE(model.data(row, ComponentListModel::SelectionKindRole).toString(), QStringLiteral("definition"));
}
```

```qml
function test_space_toggles_checkbox_without_losing_highlight() {
    list.currentIndex = list.indexForNode("occ-front-cover")
    keyClick(Qt.Key_Space)
    verify(selectionModel.isChecked("occ-front-cover"))
    compare(selectionModel.highlightedNodeId, "occ-front-cover")
}
```

- [ ] **Step 2: Run tests and verify list APIs are missing**

Run `ctest --preset windows-debug -R "component-list|export-component-list" --output-on-failure`.

Expected: model/component missing failures.

- [ ] **Step 3: Implement list roles, filters, and row facts**

Expose node ID, name, kind, path, quantity, body count, warning count, effective unit state, mirrored status, checked state, and conflict state. Filters are All, Selected, Warnings, plus case-insensitive name/path search. Preserve current highlight when filtering if the row remains visible; otherwise retain the highlight in the two viewers and announce that it is hidden by the filter.

- [ ] **Step 4: Verify mouse, keyboard, search, and screen-reader labels**

Run:

```powershell
ctest --preset windows-debug -R "component-list|export-component-list" --output-on-failure
```

Expected: grouping, quantities, search, filter, Space toggle, focus, and accessible row labels pass.

- [ ] **Step 5: Commit the component list**

```powershell
git add src/export/ComponentListModel.* src/app/qml/export/ComponentList.qml tests
git commit -m "feat: add export component list"
```

## Task 3: Implement the Dual-View Export Workspace

**Files:**
- Create: `src/app/qml/export/ExportWorkspace.qml`
- Create: `src/app/qml/export/ExportMasterView.qml`
- Create: `src/app/qml/export/IsolatedPartView.qml`
- Create: `src/app/render/GhostStateController.h`
- Create: `src/app/render/GhostStateController.cpp`
- Create: `tests/app/test_ghost_state.cpp`
- Create: `tests/qml/tst_export_workspace.qml`

- [ ] **Step 1: Write failing dual-view tests**

```qml
function test_highlight_updates_both_views_without_checking() {
    componentList.highlightNode("occ-front-cover")
    compare(masterView.focusedNodeId, "occ-front-cover")
    compare(isolatedView.nodeId, "occ-front-cover")
    verify(!exportSelection.isChecked("occ-front-cover"))
}

function test_workspace_switch_preserves_camera() {
    inspectView.cameraState = testCameraState
    application.workspace = Workspace.Export
    compare(masterView.cameraState, testCameraState)
}
```

```cpp
void GhostStateTest::focusedOccurrenceIsOpaqueAndComplementsGhosted() {
    GhostStateController controller(repeatedScene());
    controller.focus("occ-front-cover");
    QCOMPARE(controller.opacity("occ-front-cover"), 1.0f);
    QVERIFY(controller.opacity("occ-fastener-1") <= 0.30f);
}
```

- [ ] **Step 2: Run tests and verify workspace components are missing**

Run `ctest --preset windows-debug -R "ghost-state|export-workspace" --output-on-failure`.

Expected: missing controller/QML failures.

- [ ] **Step 3: Implement master and isolated scene projections**

The master view reuses the Phase 1 scene and camera, with the focused occurrence opaque, outlined, and labeled while complements use semantic ghost state. The isolated view reuses the focused definition mesh with the chosen occurrence transform removed, fits it without camera flight, and exposes orbit/fit only.

The three-column desktop layout is `300 px list | flexible master | 340 px isolated`; at narrow widths the isolated panel moves below without removing either confirmation view.

- [ ] **Step 4: Verify transparency fallback and motion**

Implement a runtime renderer mode:

```cpp
enum class GhostTechnique { Alpha, DitheredXRay };
```

Use alpha only when the benchmark and correctness probe pass. Otherwise use dithered/x-ray ghosting. Isolated replacement crossfades in 140-180 ms; reduced motion replaces immediately. Run the focused tests and a 1,000-instance ghost benchmark.

Expected: both techniques preserve focused-part identity; frame budget and picking stay correct.

- [ ] **Step 5: Commit the dual-view workspace**

```powershell
git add src/app/qml/export src/app/render tests
git commit -m "feat: add dual-view Export workspace"
```

## Task 4: Implement Naming Rules and Collision Detection

**Files:**
- Create: `src/export/NamingRules.h`
- Create: `src/export/NamingRules.cpp`
- Create: `src/app/qml/export/NamingPanel.qml`
- Create: `tests/export/test_naming_rules.cpp`
- Create: `tests/qml/tst_naming_panel.qml`

- [ ] **Step 1: Write failing CJK and collision tests**

```cpp
void NamingRulesTest::preservesCjkAndSanitizesWindowsReservedCharacters() {
    NamingRules rules;
    rules.prefix = "proto_";
    QCOMPARE(rules.apply("前盖:左", 1), QStringLiteral("proto_前盖_左"));
}

void NamingRulesTest::detectsCaseInsensitiveWindowsCollision() {
    const auto result = resolveNames({"Cover", "cover"}, PlatformRules::Windows);
    QVERIFY(result.hasBlockingCollision());
}

void NamingRulesTest::blankNameUsesStableIdFallback() {
    QCOMPARE(applyBlankFallback("", "a1b2c3d4"), QStringLiteral("part-a1b2c3d4"));
}
```

- [ ] **Step 2: Run tests and verify naming types are missing**

Run `ctest --preset windows-debug -R naming-rules --output-on-failure`.

Expected: missing implementation failure.

- [ ] **Step 3: Implement deterministic naming transformations**

Apply in order: blank-name fallback, search/replace, prefix, suffix, optional sequence, invalid-character replacement, trailing dot/space cleanup, reserved-device-name protection, extension. Never transliterate or strip CJK. Collision comparison follows destination platform case rules.

- [ ] **Step 4: Verify live preview and blocking collisions**

Run:

```powershell
ctest --preset windows-debug -R "naming-rules|naming-panel" --output-on-failure
```

Expected: CJK, blank, reserved name, sequence, search/replace, and collision preview tests pass.

- [ ] **Step 5: Commit naming rules**

```powershell
git add src/export/NamingRules.* src/app/qml/export/NamingPanel.qml tests
git commit -m "feat: add traceable export naming rules"
```

## Task 5: Build the Unit-Resolved Immutable Plan Review

**Files:**
- Create: `src/export/PlanBuilder.h`
- Create: `src/export/PlanBuilder.cpp`
- Create: `src/app/qml/export/ExportSettings.qml`
- Create: `src/app/qml/export/PlanReview.qml`
- Create: `tests/export/test_plan_builder.cpp`
- Create: `tests/qml/tst_plan_review.qml`

- [ ] **Step 1: Write failing plan-policy tests**

```cpp
void PlanBuilderTest::stepCanNormalizeToMillimetersOrInches() {
    auto settings = baseStepSettings();
    settings.outputUnit = StepOutputUnit::Inch;
    const auto plan = PlanBuilder{}.build(snapshot(), selections(), unitDecision(), settings);
    QCOMPARE(plan.rows.front().stepUnit, StepOutputUnit::Inch);
}

void PlanBuilderTest::stlIsAlwaysBinaryMillimeters() {
    auto settings = baseStlSettings();
    const auto plan = PlanBuilder{}.build(snapshot(), selections(), unitDecision(), settings);
    QVERIFY(plan.rows.front().binaryStl);
    QCOMPARE(plan.rows.front().outputUnit, OutputUnit::Millimeter);
}

void PlanBuilderTest::reviewIsBlockedByUnitOrNameIssues() {
    QVERIFY_THROWS_AS(PlanBuilder{}.build(snapshot(), selections(), suspiciousUnit(), settings()), PlanError);
    QVERIFY_THROWS_AS(PlanBuilder{}.build(snapshot(), collidingSelections(), confirmedUnit(), settings()), PlanError);
}
```

- [ ] **Step 2: Run tests and verify builder/review are missing**

Run `ctest --preset windows-debug -R "plan-builder|plan-review" --output-on-failure`.

Expected: missing API/QML failures.

- [ ] **Step 3: Implement explicit format, coordinate, grouping, and unit settings**

Settings support:

```text
STEP: source-compatible/AP242 policy, mm/in output, local/assembly coordinates,
      separate occurrences or preserve assembly, preserve names/colors.
STL: binary only, fixed mm, Draft/Standard/Fine/custom tessellation,
     optional uniform scale, separate/combined output.
Subassembly: preserve structure or flatten to one file with separate bodies.
```

`PlanReview.qml` renders one row per final file with source node, selection kind, hierarchy path, quantity, format, effective/output units, coordinates, grouping, expected bodies, final name, warnings, and blocking errors.

- [ ] **Step 4: Verify immutability and review fingerprint**

After review, serialize the exact plan and SHA-256 fingerprint. Worker execution accepts only the serialized reviewed plan; any edit returns to draft and generates a new fingerprint.

Run:

```powershell
ctest --preset windows-debug -R "plan-builder|plan-review" --output-on-failure
```

Expected: plan rows, unit policy, blocking rules, and fingerprints pass.

- [ ] **Step 5: Commit plan review**

```powershell
git add src/export/PlanBuilder.* src/app/qml/export tests
git commit -m "feat: review unit-resolved export plans"
```

## Task 6: Execute, Cancel, and Validate Plans in the Worker

**Files:**
- Modify: `src/protocol/ProtocolTypes.h`
- Modify: `src/protocol/ProtocolJson.cpp`
- Create: `src/worker/ExportSession.h`
- Create: `src/worker/ExportSession.cpp`
- Modify: `src/core/export/StepExporter.cpp`
- Modify: `src/core/export/StlExporter.cpp`
- Modify: `src/core/validation/OutputValidator.cpp`
- Create: `tests/worker/test_export_session.cpp`

- [ ] **Step 1: Write failing worker export tests**

```cpp
void ExportSessionTest::reportsProgressAndPerRowValidation() {
    WorkerHarness worker;
    const auto plan = reviewedTwoRowPlan();
    worker.sendExecutePlan(plan);
    QVERIFY(worker.events().containsType("export_progress"));
    const auto completed = worker.takeEvent("export_completed");
    QCOMPARE(completed.rows.size(), 2);
    QVERIFY(completed.rows.at(0).validation.passed);
    QVERIFY(completed.rows.at(1).validation.passed);
}

void ExportSessionTest::cancelLeavesNoFinalPartialFile() {
    WorkerHarness worker;
    worker.sendExecutePlan(slowPlan());
    worker.sendCancelExport();
    QVERIFY(QDir(outputDir()).entryList({"*.partial"}).isEmpty());
}
```

- [ ] **Step 2: Run tests and verify protocol/session failures**

Run `ctest --preset windows-debug -R export-session --output-on-failure`.

Expected: execute-plan events and worker session are missing.

- [ ] **Step 3: Add execution events and per-row state**

Add commands/events:

```text
ExecuteExportPlan(planJson, fingerprint)
CancelExport(requestId)
ExportProgress(requestId, rowIndex, stage, fraction)
ExportRowResult(requestId, rowIndex, outputPath, validation)
ExportCompleted(requestId, summary)
```

Verify the plan fingerprint in the worker before writing. Execute rows sequentially by default to cap OCCT memory; cancellation stops after the active safe point and cleans partial paths.

- [ ] **Step 4: Run round-trip and failure-injection tests**

Run:

```powershell
ctest --preset windows-debug -R "export-session|selected export|validation" --output-on-failure
```

Expected: success, wrong-unit, wrong-body-count, unwritable destination, cancellation, worker recovery, and mirrored STL cases pass.

- [ ] **Step 5: Commit worker export execution**

```powershell
git add src/protocol src/worker src/core tests/worker
git commit -m "feat: execute and validate export plans"
```

## Task 7: Write JSON/CSV Manifests and Result Recovery UX

**Files:**
- Create: `src/export/ManifestWriter.h`
- Create: `src/export/ManifestWriter.cpp`
- Create: `src/app/qml/export/ExportResults.qml`
- Create: `tests/export/test_manifest_writer.cpp`
- Create: `tests/qml/tst_export_results.qml`

- [ ] **Step 1: Write failing manifest tests**

```cpp
void ManifestWriterTest::recordsUnitOverrideAndValidation() {
    const auto manifest = ManifestWriter{}.toJson(completedPlanWithOverride());
    QCOMPARE(manifest["source"]["declaredUnits"], nlohmann::json::array({"mm"}));
    QCOMPARE(manifest["source"]["effectiveUnit"], "in");
    QCOMPARE(manifest["source"]["sourceToMillimeters"], 25.4);
    QCOMPARE(manifest["outputs"][0]["validation"]["passed"], true);
}

void ManifestWriterTest::csvPreservesCjkNames() {
    const QByteArray csv = ManifestWriter{}.toCsv(completedCjkPlan());
    QVERIFY(csv.startsWith("\xEF\xBB\xBF"));
    QVERIFY(csv.contains(QStringLiteral("前盖").toUtf8()));
}
```

- [ ] **Step 2: Run tests and verify manifest writer is missing**

Run `ctest --preset windows-debug -R manifest --output-on-failure`.

Expected: missing writer failure.

- [ ] **Step 3: Implement complete traceability fields**

JSON and CSV include source hash/path basename, schema, declared units, effective unit/factor/reason, selection kind, hierarchy path, definition quantity, output file, format, coordinates, grouping, tessellation/scale, output units, warnings, validation bodies/bounds/centroid, and pass/fail.

Write manifests atomically after all row results are collected, including failed rows.

- [ ] **Step 4: Verify results UX and retry behavior**

`ExportResults.qml` shows each output as Validated, Validated with warnings, or Failed. Failure rows show cause and retry action; retry returns to the preserved reviewed plan and re-executes only selected failed rows with a new execution ID.

Run:

```powershell
ctest --preset windows-debug -R "manifest|export-results" --output-on-failure
```

Expected: CJK, unit overrides, failed rows, accessible status announcements, and retry pass.

- [ ] **Step 5: Commit manifests and results**

```powershell
git add src/export/ManifestWriter.* src/app/qml/export/ExportResults.qml tests
git commit -m "feat: add export manifests and recovery results"
```

## Task 8: Package the Internal Pilot and Measure Workflow Outcomes

**Files:**
- Create: `packaging/windows/CMakeLists.txt`
- Create: `packaging/windows/AppxManifest.xml.in`
- Create: `packaging/macos/Info.plist.in`
- Create: `packaging/macos/notarize.ps1`
- Create: `docs/pilot/workflow-study.md`
- Create: `docs/evidence/phase-2-template.md`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add package smoke tests**

Create a CTest script that installs Loupe into a temporary prefix, verifies the shell, worker, Qt plugins, Quick 3D assets, and license notices, launches `loupe --version`, then runs a generated-fixture export through the installed worker.

Expected initial result: FAIL because install/package rules are absent.

- [ ] **Step 2: Add deterministic install and package rules**

Install the shell, worker, QML modules, Qt runtime deployment output, OCCT runtime libraries, notices, and default resources. Configure Windows MSIX/App Installer metadata and macOS `.app` bundle identifiers without embedding signing secrets.

- [ ] **Step 3: Add signing/notarization inputs through environment only**

Use:

```text
LOUPE_WINDOWS_CERT_THUMBPRINT
LOUPE_MACOS_SIGNING_IDENTITY
LOUPE_APPLE_TEAM_ID
LOUPE_NOTARY_PROFILE
```

Scripts must fail with a named message when a required release credential is missing and must never print secret values.

- [ ] **Step 4: Run pilot acceptance and workflow study**

For each participant, time the same part-export and subassembly-export tasks used in Phase 0. Record completion without assistance, wrong-unit recovery, wrong-occurrence attempts, elapsed time, validation failures, and qualitative replacement of full CAD.

Expected gate: at least 90% complete both core export tasks without assistance and show material time reduction.

- [ ] **Step 5: Commit the Phase 2 pilot checkpoint**

```powershell
git add packaging docs/pilot docs/evidence CMakeLists.txt
git commit -m "build: package and evaluate Loupe export pilot"
```

## Phase 2 Completion Gate

- Export is a distinct workspace inside the current document session.
- Highlight and checkbox state remain separate under mouse, keyboard, and filtering.
- Master and isolated views identify what the component is and where it lives.
- STEP mm/in and binary STL mm outputs validate on read-back.
- Unit overrides and scale factors are recorded in every manifest.
- Naming preserves CJK and blocks collisions.
- Every output row has an explicit validation result and recovery path.
- Signed pilot packages contain all runtime dependencies and notices.
- At least 90% of pilot users complete part and subassembly export unaided with material time reduction.
