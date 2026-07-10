# Loupe Phase 3 Hardening and Release Decision Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close pilot regressions, enforce performance and accessibility budgets, complete recovery/packaging/licensing work, and produce an evidence-backed release and P1 decision.

**Architecture:** Preserve the Phase 0–2 product boundaries and harden them through automated corpus, benchmark, UI, packaging, and recovery gates. Release artifacts are reproducible from tagged commits; signing credentials remain outside the repository; support evidence contains hashes and local diagnostics rather than source geometry.

**Tech Stack:** C++23, Qt 6, OCCT, CMake/CTest/CPack, Qt Test/Quick Test, QML profiler, platform signing/notarization tools, GitHub Actions or equivalent CI.

---

## Required Skill Preflight

- Invoke `systematic-debugging` and `test-driven-development` for every regression and fault-injection task.
- Invoke `qt-cpp-review`, `qt-qml-review`, and `qt-qml-profiler` for performance and recovery changes.
- Invoke `qt-ui-design`, `ui-ux-pro-max`, `web-design-guidelines`, and `design-motion-principles` for Task 4.
- Invoke `requesting-code-review` and `verification-before-completion` before Task 7 closes the release gate.

## File and Module Map

```text
tests/regression/                       Expanded corpus expectations and issue regressions
tests/faults/                           Worker crash, disk, cache, and corrupt-file injection
tools/bench/                            Startup/import/viewer/export benchmark runners
tools/release/check_release.py          One release-gate command
src/app/diagnostics/LocalLog.*          Redacted local user-attached logs
src/app/recovery/RecoveryController.*   Worker/document recovery UX
src/app/qml/accessibility/               Shared focus/status helpers
src/app/qml/theme/                       Complete semantic light/dark themes
packaging/windows/                       Final MSIX/update/signing configuration
packaging/macos/                         Final app/notarization/update configuration
docs/support/                            Supported inputs, logs, failure intake, SLA
docs/licenses/                           Qt/OCCT/third-party notices and review
docs/evidence/phase-3-release-report.md  Final pass/fail and P1 decision
.github/workflows/                       CI and release-gate automation
```

## Task 1: Convert Every Pilot Defect into a Named Regression

**Files:**
- Create: `tests/regression/RegressionCatalog.h`
- Create: `tests/regression/RegressionCatalog.cpp`
- Create: `tests/regression/test_regression_catalog.cpp`
- Create: `tests/regression/cases.schema.json`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write a failing catalog completeness test**

```cpp
void RegressionCatalogTest::everyBlockingPilotDefectHasExecutableCase() {
    const RegressionCatalog catalog("tests/regression/cases.json");
    for (const auto& defect : catalog.defectsBySeverity("blocking")) {
        QVERIFY2(defect.testName.has_value(), qPrintable(defect.id + " lacks testName"));
        QVERIFY2(defect.expectedOutcome.has_value(), qPrintable(defect.id + " lacks expectedOutcome"));
    }
}
```

- [ ] **Step 2: Run the test and verify the catalog is absent**

Run `ctest --preset windows-debug -R regression-catalog --output-on-failure`.

Expected: missing catalog failure.

- [ ] **Step 3: Implement the regression metadata contract**

```json
{
  "schemaVersion": 1,
  "defects": [
    {
      "id": "LOUPE-UNIT-001",
      "severity": "blocking",
      "sourceHash": "sha256-only-no-path",
      "tags": ["inch", "mislabeled", "bounds"],
      "testName": "regression_mislabeled_inch_bounds",
      "expectedOutcome": "manual_override_round_trip_passes"
    }
  ]
}
```

The catalog never contains private file contents or absolute source paths.

- [ ] **Step 4: Add generated or private-corpus executable tests for every blocking/high defect**

Run:

```powershell
ctest --preset windows-release -L regression --output-on-failure
```

Expected: all catalog entries map to discovered tests and pass; a deliberately removed mapping makes the catalog test fail.

- [ ] **Step 5: Commit the regression catalog**

```powershell
git add tests/regression tests/CMakeLists.txt
git commit -m "test: convert pilot defects into regressions"
```

## Task 2: Enforce Performance and Resource Budgets

**Files:**
- Create: `tools/bench/Budget.h`
- Create: `tools/bench/Budget.cpp`
- Create: `tools/bench/compare_benchmarks.cpp`
- Create: `tests/bench/test_budget.cpp`
- Create: `docs/performance/reference-machines.md`

- [ ] **Step 1: Write failing budget-comparison tests**

```cpp
void BudgetTest::rejectsColdLaunchRegressionOverTenPercent() {
    const Metric baseline{"cold_launch_ms", 700.0, 800.0};
    QCOMPARE(compare(baseline, 785.0).status, BudgetStatus::RegressionOverTenPercent);
}

void BudgetTest::rejectsIdleCpuAboveNearZeroThreshold() {
    const Metric idle{"idle_cpu_percent", 0.0, 0.5};
    QCOMPARE(compare(idle, 1.2).status, BudgetStatus::FailedAbsoluteTarget);
}
```

- [ ] **Step 2: Run tests and verify budget API is missing**

Run `ctest --preset windows-debug -R budget --output-on-failure`.

Expected: missing comparison implementation.

- [ ] **Step 3: Implement metric-specific absolute and regression gates**

Gate cold/warm launch, acknowledgment, tree/coarse/interaction time, cached reopen, selection p95, frame p95, idle CPU, peak memory, and export validation time. Fail a feature build if cold launch regresses more than 10% even when the absolute target still passes.

- [ ] **Step 4: Run Windows and Apple Silicon reference suites**

Run:

```powershell
build/windows-release/tools/bench/loupe_benchmark.exe corpus/private/cases.json --out evidence/performance/windows.json
build/windows-release/tools/bench/compare_benchmarks.exe --baseline evidence/baselines/windows.json --candidate evidence/performance/windows.json
```

Run on the real M-series Mac:

```bash
cmake --preset macos-release
cmake --build --preset macos-release
ctest --preset macos-release --output-on-failure
build/macos-release/tools/bench/loupe_benchmark corpus/private/cases.json --out evidence/performance/macos.json
build/macos-release/tools/bench/compare_benchmarks --baseline evidence/baselines/macos.json --candidate evidence/performance/macos.json
```

Expected: Windows and macOS reports identify reference hardware and pass every release-blocking budget.

- [ ] **Step 5: Commit benchmark enforcement**

```powershell
git add tools/bench tests/bench docs/performance
git commit -m "perf: enforce Loupe release budgets"
```

## Task 3: Add Worker Crash Recovery and Redacted Local Diagnostics

**Files:**
- Create: `src/app/diagnostics/LocalLog.h`
- Create: `src/app/diagnostics/LocalLog.cpp`
- Create: `src/app/recovery/RecoveryController.h`
- Create: `src/app/recovery/RecoveryController.cpp`
- Create: `src/app/qml/dialogs/WorkerRecoveryDialog.qml`
- Create: `tests/faults/test_worker_recovery.cpp`
- Create: `tests/app/test_local_log.cpp`

- [ ] **Step 1: Write failing recovery and redaction tests**

```cpp
void LocalLogTest::redactsUserPathAndNeverSerializesGeometry() {
    LocalLog log(tempLogRoot());
    log.importFailed(R"(C:\Users\alex\Secret\assembly.step)", "read_failed", "sha256:a1");
    const QString text = readLatestLog();
    QVERIFY_FALSE(text.contains(QStringLiteral("alex"), Qt::CaseInsensitive));
    QVERIFY_FALSE(text.contains(QStringLiteral("Secret"), Qt::CaseInsensitive));
    QVERIFY(text.contains(QStringLiteral("assembly.step")));
    QVERIFY(text.contains(QStringLiteral("sha256:a1")));
}

void WorkerRecoveryTest::restartRestoresSnapshotWithoutSilentExportResume() {
    RecoveryHarness harness;
    harness.openFixture();
    harness.crashWorkerDuringExport();
    harness.restartWorker();
    QCOMPARE(harness.documentState(), DocumentState::Viewing);
    QCOMPARE(harness.exportState(), ExportState::ReviewRequired);
}
```

- [ ] **Step 2: Run tests and verify missing diagnostics/recovery**

Run `ctest --preset windows-debug -R "local-log|worker-recovery" --output-on-failure`.

Expected: missing types fail compilation.

- [ ] **Step 3: Implement bounded, local-only JSONL diagnostics**

Each record contains UTC time, app/importer versions, platform, event code, source basename/hash, worker exit status, request ID, and redacted message. Rotate at 5 MB, retain five files, never upload automatically, and expose Reveal logs plus Attach logs to bug report.

- [ ] **Step 4: Implement explicit recovery states**

After worker failure, preserve the latest immutable snapshot, camera, visibility, highlight, checked export set, and reviewed plan. Restarting the worker reopens the source and reapplies unit interpretation. Interrupted exports always return to Review required; they never resume writing silently.

Run:

```powershell
ctest --preset windows-debug -R "local-log|worker-recovery" --output-on-failure
```

Expected: crash during import/export, restart failure, missing source after restart, and log redaction pass.

- [ ] **Step 5: Commit diagnostics and recovery**

```powershell
git add src/app/diagnostics src/app/recovery src/app/qml/dialogs tests
git commit -m "feat: recover workers with redacted local logs"
```

## Task 4: Complete Accessibility, Themes, DPI, and Reduced Motion

**Files:**
- Create: `src/app/qml/theme/ColorTokens.qml`
- Create: `src/app/qml/theme/TypeTokens.qml`
- Create: `src/app/qml/accessibility/LiveStatus.qml`
- Create: `tests/qml/tst_accessibility_complete.qml`
- Create: `tests/qml/tst_theme_contrast.qml`
- Create: `docs/ux/accessibility-checklist.md`

- [ ] **Step 1: Write failing complete-flow accessibility tests**

```qml
function test_export_flow_has_logical_focus_and_escape_routes() {
    compare(focusSequence("export"), [
        "workspaceSwitch", "componentSearch", "componentList",
        "masterView", "isolatedView", "namingGrouping", "reviewPlan"
    ])
    verify(unitDialog.closeButton.Accessible.name.length > 0)
    verify(planReview.cancelButton.visible)
}

function test_status_not_conveyed_by_color_only() {
    for (const row of exportResults.rows) {
        verify(row.statusText.length > 0)
        verify(row.statusIcon.Accessible.name.length > 0)
    }
}
```

- [ ] **Step 2: Run tests and verify incomplete tokens/helpers fail**

Run `ctest --preset windows-debug -R "accessibility-complete|theme-contrast" --output-on-failure`.

Expected: missing tokens/helpers or failed assertions.

- [ ] **Step 3: Implement semantic light/dark tokens and focus styles**

Define surface, elevated surface, primary/secondary text, border, focus, selection, warning, error, success, ghost, and viewer-background tokens for both themes. Normal text pairs meet 4.5:1 and large/glyph pairs meet 3:1. Every interactive item uses a visible 2-4 px focus ring and 44 px minimum target.

- [ ] **Step 4: Verify high DPI, CJK, text scaling, and reduced motion**

Run QML tests at 100%, 150%, and 200% scale; render representative CJK names; increase text scale; run with reduced motion. Expected: no clipped controls, hidden fixed bars, lost selection cues, or focus traps.

- [ ] **Step 5: Commit accessibility completion**

```powershell
git add src/app/qml/theme src/app/qml/accessibility tests/qml docs/ux
git commit -m "fix: complete accessible cross-platform UX"
```

## Task 5: Harden Packaging, Signing, Notarization, and Update Checks

**Files:**
- Modify: `packaging/windows/AppxManifest.xml.in`
- Create: `packaging/windows/sign.ps1`
- Modify: `packaging/macos/Info.plist.in`
- Modify: `packaging/macos/notarize.ps1`
- Create: `packaging/common/generate_update_manifest.py`
- Create: `tests/packaging/test_installed_app.ps1`
- Create: `tests/packaging/test_update_manifest.py`

- [ ] **Step 1: Write failing installed-package and update-manifest tests**

```python
def test_update_manifest_is_signed_and_version_matches(tmp_path):
    manifest = generate_manifest(package="Loupe-1.0.0.msix", version="1.0.0", key=test_key)
    assert manifest["version"] == "1.0.0"
    assert verify_signature(manifest, test_public_key)
    assert manifest["sha256"] == sha256_file("Loupe-1.0.0.msix")
```

PowerShell package smoke test must launch the installed app, open a generated STEP fixture, switch workspaces, and run one validated export through the installed worker.

- [ ] **Step 2: Run package tests and verify release rules fail**

Expected: unsigned/incomplete package and missing manifest failures.

- [ ] **Step 3: Implement final release packaging**

Windows package includes x64 shell/worker, Qt runtime/plugins/QML, OCCT runtime, VC runtime policy, notices, file associations for `.step`/`.stp`, and uninstall cleanup limited to installed files. macOS package is universal only if both architectures are actually built; otherwise explicitly Apple Silicon, hardened runtime, signed nested worker/libraries, notarized, and stapled.

- [ ] **Step 4: Implement a passive, privacy-preserving update check**

Fetch only a static signed manifest containing version, channel, URL, SHA-256, signature, and release notes URL. Send no device, file, or usage data. Offer Download update; do not install silently.

- [ ] **Step 5: Commit release packaging**

```powershell
git add packaging tests/packaging
git commit -m "build: harden signed Loupe packages"
```

## Task 6: Complete License, Notice, and Support Boundaries

**Files:**
- Create: `docs/licenses/dependency-inventory.md`
- Create: `docs/licenses/qt-lgpl-compliance.md`
- Create: `docs/licenses/occt-and-third-party.md`
- Create: `packaging/common/THIRD_PARTY_NOTICES.txt`
- Create: `docs/support/supported-inputs.md`
- Create: `docs/support/failure-intake.md`
- Create: `docs/support/pilot-sla.md`
- Create: `tools/release/check_notices.py`

- [ ] **Step 1: Write a failing dependency/notice consistency test**

```python
def test_every_shipped_dependency_has_notice():
    shipped = read_cmake_install_manifest("build/release/install_manifest.txt")
    inventory = read_inventory("docs/licenses/dependency-inventory.md")
    notices = read_notices("packaging/common/THIRD_PARTY_NOTICES.txt")
    for dependency in detect_dependencies(shipped):
        assert dependency in inventory
        assert dependency in notices
```

- [ ] **Step 2: Run the test and verify documentation is incomplete**

Expected: missing inventory/notices failure.

- [ ] **Step 3: Document reviewed license and relinking obligations**

Record exact Qt, OCCT, compiler runtime, and third-party versions, license identifiers, source-offer/relinking requirements, dynamic-linking evidence, notices, and reviewer/date. Do not claim legal approval from automation; record the named human review result.

- [ ] **Step 4: Publish support and failure-intake boundaries**

Supported inputs document schemas, platforms, units, hierarchy limitations, external references, and non-goals. Failure intake requests the original file only through the approved internal channel, plus Loupe logs and reproduction steps; blocking import failures retain the two-business-day pilot SLA.

- [ ] **Step 5: Commit licenses and support docs**

```powershell
git add docs/licenses docs/support packaging/common tools/release/check_notices.py
git commit -m "docs: define Loupe release and support obligations"
```

## Task 7: Automate the Full Release Gate

**Files:**
- Create: `tools/release/check_release.py`
- Create: `.github/workflows/ci.yml`
- Create: `.github/workflows/release-candidate.yml`
- Create: `docs/evidence/phase-3-release-report.md`
- Create: `docs/product/p1-decision.md`

- [ ] **Step 1: Write failing release-check aggregation tests**

```python
def test_release_gate_fails_when_any_required_gate_is_open():
    result = check_release({
        "tests": "pass",
        "windows_benchmark": "pass",
        "macos_benchmark": "open",
        "regressions": "pass",
        "packages": "pass",
        "licenses": "pass"
    })
    assert result.exit_code == 1
    assert "macos_benchmark" in result.open_gates
```

- [ ] **Step 2: Run aggregation tests and verify checker is missing**

Expected: missing checker failure.

- [ ] **Step 3: Implement one deterministic release-candidate command**

```powershell
python tools/release/check_release.py `
  --build-dir build/release `
  --evidence evidence `
  --package-dir dist `
  --report docs/evidence/phase-3-release-report.md
```

The command runs unit/integration/QML/regression/package tests, benchmark comparisons, notice checks, manifest signature verification, and gate completeness. It exits nonzero for any open required gate and writes the same result locally and in CI.

- [ ] **Step 4: Record the release and P1 decision from evidence**

`phase-3-release-report.md` records pass/fail for every target with artifact hashes. `p1-decision.md` lists observed pilot requests, frequency, workflow impact, performance/support cost, decision, and rationale. Only PRD P1 items backed by pilot evidence may be approved.

- [ ] **Step 5: Commit the release gate**

```powershell
git add tools/release .github/workflows docs/evidence/phase-3-release-report.md docs/product/p1-decision.md
git commit -m "ci: enforce Loupe release decision gate"
```

## Phase 3 Completion Gate

- Every blocking/high pilot defect maps to a passing regression.
- Absolute performance targets and the 10% cold-launch regression rule pass on Windows and real Apple Silicon hardware.
- Worker crash recovery preserves document state without silently resuming exports.
- Logs are local, bounded, redacted, and user-attached only.
- Light/dark, keyboard, screen reader, CJK, high-DPI, and reduced-motion checks pass.
- Installed signed/notarized packages pass end-to-end smoke export.
- Dependency licenses/notices and support boundaries have named human review.
- The automated release checker reports no open required gates.
- Release and P1 decisions cite pilot and regression evidence rather than feature speculation.
