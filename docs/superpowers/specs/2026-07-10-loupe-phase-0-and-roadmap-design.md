# Loupe Phase 0 and Product Roadmap Design

**Date:** 2026-07-10

**Status:** Approved; implementation plans created

**Source of truth:** [Loupe — STEP Assembly Inspector PRD v0.3](https://app.notion.com/p/399f19f7357c8127a4eecfd097bdd35d)

**Interim owner:** Project user for product, architecture, and corpus

**Delivery rule:** Advance by evidence gates as quickly as credible results allow; no arbitrary calendar timebox

## 1. Purpose

Loupe is a local-first desktop application for engineers and prototype technicians who need to inspect STEP assemblies and export selected geometry without opening a full CAD suite. It is an inspection and selective-export tool, not a CAD editor.

This design defines:

- The Phase 0 feasibility spike.
- The production architecture that Phase 0 must de-risk.
- The Inspect and Export workspace experience.
- Unit recognition, correction, conversion, and traceability.
- Reliability, validation, performance, and accessibility contracts.
- The feature goals and exit criteria for Phases 0 through 3.

Four implementation plans accompany this design: one plan for each phase.

## 2. Confirmed Decisions

1. Loupe will use a new native shell. Mayo remains a reference implementation and benchmark source only.
2. The production stack remains C++20/23, Qt 6 Quick/QML, Qt Quick 3D, OCCT XDE/XCAF, CMake, and Ninja.
3. Phase 0 uses an evidence-split approach:
   - A headless native spike proves CAD traversal, unit handling, export, and validation.
   - A separate interactive prototype validates the product experience and motion language.
4. Real internal STEP files will live under `corpus/private/` and are excluded from Git.
5. Windows 11 x64 is the active Phase 0 evidence platform. The architecture and build remain Apple Silicon-ready, but the macOS performance gate stays open until M-series hardware is available.
6. Loupe has two explicit workspaces within the same open document and application session:
   - **Inspect** is the default workspace.
   - **Export** is a deliberate review workspace.
7. Viewer highlight is transient. Export checkboxes define the persistent export set.
8. Unit uncertainty blocks export until reviewed. Unit correction never mutates the source STEP file.
9. Motion is restrained, interruptible, accessibility-aware, and used only to explain state or spatial continuity.

## 3. Product Boundary

### In scope

- STEP AP203, AP214, and AP242 import supported by OCCT.
- Structured assembly, flat multi-solid, single-part, partial/invalid, and external-reference classification.
- Assembly tree, search, repeated-definition awareness, visibility, selection, and warnings.
- 3D navigation, measurement, sectioning, isolate, ghost, and high-resolution capture.
- Selection-based STEP and binary STL export.
- Occurrence, unique-definition, body, and subassembly selection.
- Local and assembly coordinate policies.
- Preserve-assembly and flatten-to-multi-body treatments.
- Unit recognition, suspicion warnings, manual interpretation, custom scale factors, and output conversion.
- Naming rules, CJK preservation, collision detection, plan review, read-back validation, and JSON/CSV manifest.
- Windows 11 x64 and Apple Silicon macOS release support.

### Out of scope

- Geometry editing, feature reconstruction, sketches, mates, or design-intent recovery.
- Healing, sewing, boolean fusion, interference analysis, or fused-solid part reconstruction.
- Native proprietary CAD formats.
- Slicing, supports, printer control, cloud processing, or collaboration.
- Revision compare, BOM export, print-bed fit checks, or telemetry.
- General CAD authoring controls in either workspace.

## 4. System Architecture

### 4.1 Phase 0 architecture

```text
corpus/private/*.step
        |
        v
loupe-spike CLI
        |
        v
cad-core (the only owner of OCCT objects)
        |
        +--> import/classify/unit evidence
        +--> XCAF traversal and stable IDs
        +--> selection and export plan
        +--> temporary export document
        +--> STEP/STL write
        +--> read-back validation
        |
        v
assembly snapshot JSON + validation JSON + benchmark CSV + failure report
```

The interactive Phase 0 prototype consumes deterministic fixture snapshots shaped like the future worker protocol. It does not claim to prove CAD correctness.

### 4.2 Production boundaries

#### `cad-core`

- Owns OCCT/XCAF documents, shapes, labels, tessellation, STEP/STL translation, and validation.
- Exposes stable domain values rather than OCCT handles.
- Has no QML, Qt Quick, or renderer dependency.
- Supports both the Phase 0 CLI and the future worker process.

#### `loupe-worker`

- Hosts `cad-core` out of process.
- Receives versioned commands and emits versioned events.
- Supports cancellation, progress, failure isolation, and predictable memory reclamation.
- Streams tree data and coarse meshes before refinement completes.

#### `loupe-shell`

- Owns application state, workspaces, keyboard navigation, presentation models, and command routing.
- Never receives OCCT objects.
- Treats imported facts, warnings, unit evidence, and validation results as immutable worker outputs.

#### `loupe-viewer`

- Uses Qt Quick 3D behind a renderer interface.
- Accepts mesh buffers, transforms, stable IDs, material/color data, selection state, and visibility state.
- Reuses definition meshes through GPU instancing.
- Supports a later custom batched renderer without changing workspace or worker contracts.

#### `export-module`

- Loads on first entry to the Export workspace and remains resident until application exit.
- Owns export selection, naming/grouping, output-unit policy, plan review, execution progress, and results.
- Builds temporary export documents and never changes the imported source document.

#### `loupe-cache`

- Uses SQLite metadata plus versioned compressed mesh/snapshot files.
- Lives in the per-user local app-data directory, never a network or synced directory.
- Keys entries by file hash, size, mtime, importer version, mesh profile, and effective unit interpretation.
- Uses a 2 GB default LRU budget and exposes only Clear cache.

### 4.3 Core domain contracts

The UI and worker communicate through explicit values:

- `SourceAsset`: path identity, file hash, size, mtime, schema, and classification.
- `UnitEvidence`: declared representation units, XCAF unit, normalized system unit, effective interpretation, scale factor, confidence state, reason, and user acknowledgment.
- `AssemblyNode`: stable ID, node kind, name, hierarchy path, parent ID, definition ID, occurrence placement, body references, color, and warnings.
- `AssemblySnapshot`: roots, nodes, definitions, occurrences, bodies, units, warnings, and progressive-load status.
- `MeshPacket`: definition ID, refinement level, vertex/index buffers, bounds, winding/mirror information, and material slots.
- `SelectionRef`: stable node ID plus selection kind: occurrence, definition, body, or subassembly.
- `ExportPlan`: immutable reviewed list of output rows and all policies required to reproduce them.
- `ValidationResult`: per-output read-back status, validity, bodies, bounds, placement, units, warnings, and errors.
- `ManifestRecord`: traceability from source selection to final output and validation result.

Stable IDs must be deterministic for the same source content and importer version. They must not depend on traversal memory addresses or presentation order.

## 5. Unit Trust Model

### 5.1 Import behavior

1. Read every length unit reported by STEP representations before transfer.
2. Read the length-unit attribute placed in the XCAF document.
3. Normalize geometry into an internal millimeter system for inspection and comparison.
4. Record declared and effective units separately.
5. Calculate normalized model extents and present them in the file-identity area.

OCCT unit conversion does not eliminate the need to preserve source evidence. Loupe must report what the file declared, what OCCT normalized, and what Loupe uses effectively.

### 5.2 Confidence states

- **Confirmed:** coherent unit metadata exists and the user has not overridden it.
- **Suspicious:** metadata exists, but extents or known conversion ratios suggest a possible authoring/export mismatch.
- **Missing or mixed:** no usable single interpretation can be established automatically; export is blocked until resolved.
- **User override:** the user explicitly chose an alternate unit or custom scale factor.

Size-based checks are suggestions, never automatic verdicts. Loupe may show an alternate inch/mm preview but must never silently apply it.

The initial suspicion rule is intentionally conservative: flag an assembly when its normalized longest extent is below 10 mm or above 100,000 mm, or when importer evidence is internally inconsistent. Phase 0 measures false positives and false negatives across the real corpus; later threshold changes are versioned importer behavior. Models inside that range are not assumed correct—the unit chip remains visible and manual review is always available.

### 5.3 Manual correction

The source-unit review shows:

- Declared STEP units.
- Current effective interpretation.
- Current extents.
- Alternate inch/mm extents when a 25.4 relationship is plausible.
- A manual Interpret source values as choice.
- An advanced custom scale factor.
- Corrected extents before applying.

An override is local and non-destructive. It is stored against file hash, size, and mtime and invalidates when the source changes.

### 5.4 Measurement and export

- Measurements use effective units and display an overridden marker when applicable.
- Unresolved suspicious, missing, or mixed units block final export writing.
- STEP outputs support normalized millimeters or inches.
- Binary STL outputs are always millimeters.
- The optional STL uniform scale factor remains explicit and is applied after effective-unit normalization.
- Read-back checks output units and bounds against the reviewed interpretation.
- The manifest records declared units, effective units, source-to-internal factor, manual factor, output units, and acknowledgment.

## 6. Experience Design

### 6.1 Shared shell

The top application bar contains:

- Loupe identity and open-file identity.
- Classification and effective-unit status.
- A two-item workspace switcher: Inspect and Export.
- Command search.

There is no conventional CAD ribbon. The shell uses a restrained dark technical palette by default, with a complete light theme and semantic design tokens. Typography prioritizes legibility, CJK coverage, compact labels, and tabular figures for dimensions and progress.

### 6.2 Inspect workspace

Inspect is the default workspace.

```text
+----------------------+----------------------------------------------+
| searchable assembly  |                                              |
| tree                 |              master viewer                   |
| visibility           |                                              |
| warnings             |        [floating inspection toolbar]         |
+----------------------+----------------------------------------------+
```

The floating bottom toolbar provides Fit, Isolate, Section, Measure, Ghost, and Capture. It remains in a stable location. Selection changes availability and contextual labels without moving the dock.

When a mode such as Section is active, the dock transforms in place to show axis, reverse, cap, and position controls. Closing the mode restores the default dock.

Viewer selection and tree selection are synchronized. Export state is not edited in this workspace except through the explicit transition to Export.

### 6.3 Export workspace

Export is a separate workspace inside the same document session, replacing the PRD's earlier single contextual export surface.

```text
+----------------------+---------------------------+------------------+
| component list       | master assembly viewer    | isolated viewer  |
| search and filters   | selected part opaque      | active part only |
| export checkboxes    | complements ghosted       | facts/warnings   |
| occurrence facts     | hierarchy-path label      | add/remove       |
+----------------------+---------------------------+------------------+
| selection count | naming and grouping | review export plan          |
+---------------------------------------------------------------------+
```

#### Selection rules

- Highlighting a component is transient and never changes the export set.
- Highlight updates both viewers:
  - The master assembly viewer keeps the focused component fully opaque and outlined while other components become semi-transparent.
  - The isolated viewer shows only the focused component, fit for confirmation.
- The component list uses semantic checkboxes to add or remove export selections.
- The isolated-view action mirrors the checkbox state with clear Add to export or Remove from export wording.
- Repeated definitions show quantity and allow definition-level or occurrence-level choices.
- Each row exposes its selection type, hierarchy path, warnings, unit status, body count, and mirrored-instance status where relevant.

The dual view answers two separate questions: what is this component, and where is it in the assembly?

### 6.4 Ghost rendering

- The selected component remains fully opaque and receives an outline plus a text label, so color is not the only selection cue.
- Complementary geometry uses a semi-transparent, desaturated ghost treatment.
- Phase 1 benchmarks transparency sorting, repeated instancing, mirrored transforms, and frame time.
- If conventional transparency cannot meet the frame budget or produces ordering errors, the renderer uses a visually equivalent dithered/x-ray treatment rather than removing assembly context.

### 6.5 Motion system

Motion explains cause and continuity:

- Tree/view selection: 120 ms color and outline transition.
- Contextual panel or toolbar transformation: 160-180 ms opacity plus at most 8 px translation.
- Isolated preview replacement: 140-180 ms crossfade.
- Workspace transition: no camera flight; the viewer state persists while surrounding controls crossfade within 180-220 ms.
- Exit transitions are approximately 70% of entry duration.
- Import/refinement uses determinate progress rather than decorative pulsing.

Motion must be interruptible and must not block input. Animations use transform and opacity, avoid bounce/elastic curves, and respect reduced-motion settings. Reduced motion switches state immediately while preserving focus, labels, and selection meaning.

## 7. Data Flows

### 7.1 Import and inspection

1. The shell acknowledges an open/drop request within the file-drop budget.
2. The worker reads the file, captures unit evidence, classifies the input, and emits warnings.
3. The worker streams root/tree facts and coarse definition meshes.
4. The shell renders the tree and first useful view.
5. Visible geometry is prioritized for refinement.
6. Tree and viewport selection operate on stable IDs.
7. Measurement and sectioning remain presentation-only and never change source geometry.

### 7.2 Unit correction

1. A suspicious, missing, or mixed-unit state opens the unit review gate.
2. Loupe shows declared metadata and current extents.
3. The user previews alternate inch/mm or custom-factor interpretations.
4. Applying an override rebuilds effective transforms, dimensions, meshes/cache keys, and export assumptions without changing the source.
5. The user's decision is recorded for the current source identity.

### 7.3 Export

1. The user enters Export with the same camera, visibility, and active selection where possible.
2. Highlighting list rows updates the master ghost state and isolated preview.
3. Checkboxes build the persistent export set.
4. Naming, grouping, coordinates, format, output units, and structure handling produce an immutable plan.
5. The user reviews every output row and resolves collisions, warnings, and unit gates.
6. The worker creates a temporary document and writes outputs.
7. Each output is read back and validated independently.
8. Loupe produces JSON/CSV manifests and an explicit success, warning, or failure result per row.

## 8. Reliability and Error Handling

### 8.1 Import states

`Acknowledged -> Classifying -> Unit review if needed -> Tree ready -> Coarse view -> Refined`

Terminal or partial states include:

- Partial with named warnings.
- Invalid or unsupported.
- Missing external references.
- Mixed or unresolved units.
- Canceled.
- Worker failure.

Usable partial geometry remains inspectable. Warnings identify the affected reference, node, or capability. Cancellation leaves no corrupt cache entry. Worker crashes produce a recoverable document state and local diagnostic report.

### 8.2 Export states

`Selection draft -> Unit-resolved plan -> Plan reviewed -> Writing -> Read-back -> Validated | Validated with warnings | Failed`

- Batch status never hides a failed output row.
- Failed outputs remain listed with cause and recovery action.
- Retry starts from the reviewed plan and preserved selections.
- The source STEP file is read-only and is never used as an output destination.
- Partial file writes use temporary paths and atomic finalization where supported.

## 9. Verification Strategy

### Unit tests

- Stable ID generation.
- XCAF traversal and node-kind classification.
- Definition/occurrence grouping and placements.
- Unit evidence, normalization, overrides, and invalidation.
- Selection and export-plan construction.
- Naming, sanitization, CJK preservation, and collision detection.
- Coordinate and transform policies.

### Corpus regression

Every corpus case has metadata describing its expected classification and supported outcomes. The corpus covers:

- Structured, flat multi-solid, single-part, multiple-root, partial, damaged, and external-reference files.
- AP203, AP214, and AP242.
- Deep hierarchy, repeated definitions, legitimate multi-body parts, CJK names, and mirrored instances.
- Correct millimeter, correct inch, missing-unit, mixed-unit, and intentionally mislabeled cases.
- At least five inch-unit files and at least three CJK-named assemblies before pilot.

Golden structure snapshots are versioned outside the private geometry where possible. Differences require an explicit importer-version review.

### Round-trip validation

- Output reopens successfully and is non-null.
- BREP validity passes where applicable.
- Body/solid/shell counts are recorded and compared.
- Bounds, centroid, and placement match reviewed policy within defined tolerances.
- Units match the output plan.
- Mirrored STL winding is correct.
- Manifest linkage is complete.

### UX automation

- Inspect is the default workspace.
- Workspace switching preserves camera and selection context.
- Highlight never changes checkbox/export state.
- Checkbox and isolated-view actions remain synchronized.
- Keyboard focus order, screen-reader names, warnings, and recovery actions are correct.
- Unit review blocks export until resolved.
- Reduced motion preserves usability.

### Performance harness

Measure cold/warm launch, file acknowledgment, first tree, first coarse view, first useful interaction, cache reopen, selection response, frame time, peak memory, idle CPU, unit-override rebuild, and export/validation duration.

Reference targets remain:

- Cold usable shell under 800 ms.
- Warm usable shell under 300 ms.
- File-drop acknowledgment under 50 ms.
- Small first coarse view under 1 second.
- Representative medium first useful view within 2-3 seconds.
- Large first useful interaction under 5 seconds.
- Cached tree and mesh under 1 second.
- Typical selection under 50 ms.
- Typical navigation at 60 fps.
- Idle CPU near zero.

## 10. Phase Goals, Features, and Exit Criteria

### Phase 0: Feasibility and contracts

**Goal:** Prove geometry and unit correctness and lock reusable product/worker contracts before production UI investment.

**Features and work:**

- Private corpus layout and case metadata.
- Windows native toolchain and reproducible CMake dependency setup.
- Headless OCCT/XCAF import, classification, traversal, and stable IDs.
- Declared-unit capture, internal normalization, suspicion evidence, and manual override contract.
- Selected occurrence/definition STEP and STL export.
- Temporary export documents and read-back validation.
- JSON snapshots, validation reports, benchmark CSV, and failure classifications.
- Interactive Inspect/Export prototype with dual viewers and motion specification.
- Architecture boundary and protocol contract for the future worker.

**Exit criteria:** Every corpus case yields an explicit result. Supported cases pass structured traversal, unique-definition/occurrence grouping, selected export, unit conversion, and read-back validation. Failures are named, reproducible, and retained. The new-shell decision is already final.

### Phase 1: Cross-platform inspection shell

**Goal:** Deliver a responsive, trustworthy inspection workflow on the new Loupe shell.

**Features and work:**

- Qt/QML application shell and isolated native CAD worker.
- Versioned local IPC and cancellation.
- Progressive tree and coarse-to-fine mesh streaming.
- Qt Quick 3D renderer abstraction and definition instancing.
- Inspect workspace with searchable tree and floating toolbar.
- Tree/view synchronization, fit, hide, isolate, ghost, and restore.
- Common measurement, one section plane, and high-resolution capture.
- Unit identity chip, warnings, alternate preview, manual override, and effective-unit measurement.
- Local cache foundation.
- Windows benchmarks and macOS build/performance verification when M-series hardware is available.

**Exit criteria:** A pilot user opens a representative assembly, resolves any unit warning, selects and inspects components, measures and sections geometry, and navigates smoothly. Measurements match the reviewed effective units. Windows performance targets are measured; the macOS release gate cannot close without real M-series evidence.

### Phase 2: Selective export pilot

**Goal:** Make intended exports obvious before writing and trustworthy after validation.

**Features and work:**

- Export workspace with searchable/filterable component list.
- Master assembly viewer with focused-part highlight and ghosted context.
- Isolated component viewer with synchronized Add/Remove export state.
- Occurrence, definition, body, and subassembly selection.
- STEP output in normalized millimeters or inches.
- Binary STL in fixed millimeters with optional uniform scale factor.
- Local/assembly coordinates, preserve/flatten grouping, and per-occurrence policies.
- Naming rules, CJK preservation, preview, and collision handling.
- Unit-resolved immutable export plan.
- Per-output read-back validation and JSON/CSV manifest.
- Signed Windows installer and notarized macOS package for pilot.
- Workflow study against Phase 0 CAD-suite baselines.

**Exit criteria:** At least 90% of pilot users export a selected part and subassembly without assistance. Outputs have the intended scale, units, geometry, names, quantity, and placement. The workflow demonstrates a material time reduction from the Phase 0 baseline.

### Phase 3: Hardening and release decision

**Goal:** Convert pilot evidence into a stable release boundary and evidence-based P1 decision.

**Features and work:**

- Expanded corpus and regression closure.
- Import, unit, export, renderer, memory, startup, and cache hardening.
- Worker crash recovery and local user-attached diagnostic logs.
- Accessibility, keyboard, high-DPI, light/dark, and reduced-motion completion.
- Windows signing, macOS notarization, installer/update behavior, and release checks.
- Qt/OCCT license and notice review.
- Support, failure-file intake, defect triage, and compatibility boundaries.
- P1 decision based on observed pilot use rather than speculative demand.

**Exit criteria:** Release performance and quality targets are met on reference hardware, no known silent geometry-loss or unit-loss defect remains, supported inputs and failure behavior are documented, and the release/P1 decision is recorded.

## 11. Dependencies and Risks

### Dependencies

- Real internal STEP corpus under `corpus/private/`.
- Windows C++ toolchain, CMake, Ninja, Qt 6, and OCCT.
- Apple Silicon hardware before the macOS performance gate can close.
- Reference Creo workflow timing for three representative tasks.

### Key risks and mitigations

- **Hierarchy or metadata is absent:** classify truthfully and expose explicit flat-model fallback.
- **Mislabeled units look plausible:** never auto-correct; show declared/effective units and require review when evidence is uncertain.
- **Ghost transparency is slow or incorrectly sorted:** benchmark early and fall back to a dithered/x-ray ghost treatment.
- **Large assemblies overwhelm the viewer:** stream, prioritize visible geometry, instance definitions, and retain a renderer boundary.
- **Occurrence and definition choices confuse users:** show kind, quantity, hierarchy path, and dual-view context before checking export.
- **Export changes geometry, units, or placement:** use temporary documents, immutable plans, mandatory read-back, and manifests.
- **Scope drifts toward CAD authoring:** enforce the feature admission guardrails and binding cuts from the PRD.
- **Cross-platform quality remains unproven:** keep the macOS gate explicitly open until tested on M-series hardware.

## 12. Project Skill Stack

The following project-local skills are installed under `.agents/skills/`:

- `qt-cmake-project`
- `qt-cpp-review`
- `qt-qml`
- `qt-qml-review`
- `qt-qml-test`
- `qt-qml-test-run`
- `qt-qml-profiler`
- `qt-ui-design`
- `design-motion-principles`

Implementation will also use the existing brainstorming, writing-plans, test-driven-development, systematic-debugging, requesting-code-review, verification-before-completion, frontend-design, web-design-guidelines, and ui-ux-pro-max workflows at their appropriate gates.

## 13. Planning Deliverables After Spec Review

Written-spec approval authorizes creation of four implementation plans:

1. Phase 0 feasibility and contracts plan.
2. Phase 1 inspection shell plan.
3. Phase 2 selective export pilot plan.
4. Phase 3 hardening and release-decision plan.

Each plan will contain goals, scoped features, file/module boundaries, test-first tasks, dependencies, evidence gates, verification commands, and exit criteria. Phase 0 is executed first; later plans remain roadmap artifacts until their preceding phase exits.
