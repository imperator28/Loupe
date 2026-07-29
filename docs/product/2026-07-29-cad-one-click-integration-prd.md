# Loupe One-Click CAD Integration — Product Requirements

**Date:** 2026-07-29

**Status:** Approved product direction; ready for Windows implementation spikes

**Product phase:** Post-v0.1.3 integration workstream

**Target platform:** Windows 11 x64

**Initial CAD hosts:** SOLIDWORKS and Creo Parametric

**Excluded host:** Fusion 360

**Scope:** One-time, one-click transfer of the active CAD model into Loupe's split-export workflow

**Owner:** Project user for product; Windows implementation owner to be assigned

---

## Executive Decision

Build two thin Windows CAD add-ins around one vendor-neutral Loupe handoff:

1. **Send to Loupe for SOLIDWORKS**
2. **Send to Loupe for Creo**
3. A shared `Loupe.exe --handoff <directory>` contract

Each add-in uses its CAD application's native STEP translator. It writes one
self-contained STEP AP214 file plus one small JSON handoff manifest into a
temporary directory, starts Loupe, and returns control to CAD. Loupe validates
the handoff, imports the STEP file with its existing out-of-process OpenCascade
worker, and opens directly in the Export workspace.

This is a **one-time transfer**, not LiveLink:

- no continuous update;
- no source-model watcher;
- no synchronization after export;
- no bidirectional editing;
- no requirement for Loupe IDs to survive another CAD export.

The data authority is deliberately narrow:

- **STEP is authoritative** for geometry, topology, hierarchy, placements,
  names, units, and original colors.
- **`handoff.json` is authoritative only** for transfer context that STEP
  cannot represent reliably.
- The manifest MUST NOT duplicate the CAD assembly tree.
- The package MUST NOT contain one STEP file per component.

This preserves useful source data without creating two competing models of the
assembly or a directory of fragile cross-file references.

---

## Feasibility Summary

**Overall feasibility: high, with a staged delivery recommended.**

| Area | Feasibility | Reason |
| --- | --- | --- |
| Native STEP export | High | Both CAD hosts expose supported STEP export paths. |
| One-click ribbon command | High | Both hosts support in-process add-ins and command/ribbon integration. |
| Geometry, hierarchy, units, and names | High | Already represented in STEP and already consumed by Loupe's XCAF importer. |
| Original part and face colors | High with certification | AP214 supports colors; both translators expose appearance export, but override precedence needs corpus testing. |
| Hidden/suppressed policy | Medium-high | Product behavior is decided, but each adapter must prove its native translator obeys it. |
| SOLIDWORKS adapter | High | Mature COM automation surface and native STEP translator. |
| Creo adapter | Medium-high | Supported native TOOLKIT export path, but development licensing, binary/version compatibility, application unlocking, and export-profile restoration are release gates. |
| Shared Loupe handoff | High | Small extension to the existing Windows app and import controller. |
| Cross-vendor metadata parity | Medium | Core data is portable; arbitrary custom properties are not consistent enough for v1. |

### Recommended sequencing

Build the shared handoff receiver first, then SOLIDWORKS, then Creo. SOLIDWORKS
provides the fastest path to validate the end-to-end product experience. Creo
should begin with a time-boxed SDK/licensing/profile-isolation spike before its
production adapter is scheduled.

---

## Product Outcome

An engineer working in SOLIDWORKS or Creo can move the active part or assembly
into Loupe for splitting without seeing or configuring an intermediate file.

The experience is:

```text
CAD ribbon: Send to Loupe
        |
        | native, silent STEP AP214 export
        v
temporary handoff/model.step + handoff.json
        |
        | Loupe.exe --handoff "<handoff directory>"
        v
Loupe validates and imports in its worker
        |
        v
Export workspace, with hierarchy and original colors ready for splitting
```

The user MAY notice a short progress notification, but MUST NOT be asked to:

- select STEP as a format;
- choose an AP protocol;
- choose a temporary destination;
- manage a ZIP or intermediate directory;
- reconfigure their permanent CAD export preferences;
- locate the exported STEP file in Loupe.

---

## Users and Jobs

### The CAD designer — "get this assembly into Loupe now"

| Job | Requirement it drives |
| --- | --- |
| "I already have the correct configuration open." | Export the active configuration or simplified representation. |
| "Hiding something should not accidentally delete it from the transfer." | Include hidden components. |
| "Suppressed means it is not part of this design state." | Omit suppressed and explicitly excluded components. |
| "I need to recognize parts immediately." | Preserve source part, occurrence, and face colors. |
| "I should not need to understand STEP settings." | One ribbon button and a controlled export profile. |
| "Do not rearrange or flatten my assembly." | Preserve hierarchy, transforms, and repeated component instances. |

### The Loupe user — "split the right bodies confidently"

| Job | Requirement it drives |
| --- | --- |
| "Show me exactly what CAD transferred." | Display transfer source, representation, counts, and any degradation warning. |
| "Let me tell repeated parts apart." | Preserve occurrence identity and transforms. |
| "Let me tell nearby parts apart." | Preserve original colors; never overwrite valid source colors by default. |
| "Take me to the work I came to do." | Land in Export, not the generic open-file state. |
| "Do not leave mystery files on my computer." | Loupe owns cleanup after accepting the handoff. |

### The Windows developer — "add vendors without changing Loupe each time"

| Job | Requirement it drives |
| --- | --- |
| "Test the receiver without owning CAD seats." | The handoff is a documented, vendor-neutral file contract with generated fixtures. |
| "Keep vendor SDKs out of the Loupe process." | CAD adapters launch Loupe; Loupe never loads CAD SDK libraries. |
| "Ship vendor updates independently." | Adapters are separate deliverables using the same versioned handoff schema. |
| "Diagnose failures without customer CAD data." | Structured, redacted diagnostics and stable error codes. |

---

## Scope

### In scope for v1

1. Windows 11 x64.
2. SOLIDWORKS part and assembly documents.
3. Creo Parametric part and assembly models.
4. One command named **Send to Loupe** in each host's ribbon/command manager.
5. Native, silent export to a single STEP AP214 file.
6. Exact B-rep geometry, not a tessellated-only transfer.
7. Active configuration in SOLIDWORKS.
8. Active simplified representation in Creo.
9. Hidden components included.
10. Suppressed and explicitly excluded components omitted.
11. Assembly hierarchy and occurrence transforms preserved.
12. Repeated component definitions preserved as repeated instances.
13. Multi-body parts preserved for Loupe's existing body splitting.
14. Source names and length units preserved.
15. Original part, body, occurrence, and face colors preserved where the
    source translator can express them.
16. Minimal versioned JSON manifest.
17. Direct launch into Loupe's Export workspace.
18. Progress, cancellation before launch, actionable failure messages, and
    cleanup of stale handoffs.
19. Separate signed installers for Loupe and each CAD adapter, or one Windows
    bootstrapper that installs selectable components.

### Explicit non-goals

- Continuous synchronization, automatic update, or LiveLink.
- Sending changes from Loupe back into CAD.
- Feature history, design tree features, sketches, equations, or parametric
  constraints.
- Mates and assembly constraints.
- PMI, GD&T, annotations, drawings, views, or title blocks.
- Datums, axes, points, construction geometry, cross-sections, cable surfaces,
  or wireframe-only entities.
- Sheet-metal unfolding.
- Custom property synchronization in v1.
- PDM/PLM check-in, checkout, revision, or vault integration.
- Configuration comparison or batch export of multiple configurations.
- User-selectable STEP settings in the primary workflow.
- External-reference STEP packages or a component-per-file package.
- Fusion 360.
- macOS CAD adapters.

---

## Locked Product Decisions

### Active-model scope

The transfer represents the active CAD design state:

- **SOLIDWORKS:** active part or active assembly configuration.
- **Creo:** active part or active assembly simplified representation.
- Hidden components are included.
- Suppressed components are omitted.
- Components explicitly excluded by the active representation are omitted.
- The master assembly outside the active representation is not exported.

Hiding is treated as a visual state. Suppression/exclusion is treated as a
design-state decision.

### STEP protocol

Use **STEP AP214** as the v1 interoperability baseline.

AP214 is selected because color preservation is mandatory and SOLIDWORKS
documents body, face, and curve color support for AP214 while AP203 has no
color implementation. Creo exposes AP214, appearances/layers, material
definition, validation information, and single-file assembly export in its
STEP profile.

AP242 MAY be evaluated later, but MUST NOT enter the v1 contract until the same
profile can be produced and certified across both vendors without requiring an
MBD-specific workflow or adding unwanted PMI.

### Package shape

The transfer uses a temporary directory as an ownership boundary, not as a
user-facing archive:

```text
%LOCALAPPDATA%\Loupe\Handoffs\<uuid>\
    model.step
    handoff.json
```

There is:

- no ZIP;
- no external STEP reference;
- no user-visible "exported package";
- no one-file-per-part structure;
- no copy of the source CAD file.

### Color is required source data

Color preservation is a v1 acceptance requirement. It is not decorative
polish.

Loupe resolves display color in this precedence:

1. explicit face color;
2. component-occurrence color override;
3. part/body definition color;
4. Loupe fallback only when the STEP source contains no applicable color.

Loupe MUST NOT assign arbitrary differentiation colors over valid source
colors. A future optional visualization mode may synthesize distinct colors,
but that is separate from source fidelity.

---

## Export Profile Contract

Every adapter MUST produce the same logical transfer profile, even where the
vendor's setting names differ.

| Concern | Required value |
| --- | --- |
| Protocol | STEP AP214 |
| Assembly packaging | One self-contained file |
| Geometry | Exact "as-is" solid/surface B-rep |
| Tessellation-only representation | Off |
| Assembly structure | Preserved |
| Repeated components | Preserved as instances |
| Units | Source document length unit |
| Coordinate system | Default model coordinate system; no added transform |
| Names | Part, assembly, and occurrence names where supported |
| Appearances | On |
| Face/edge properties | On for the certification spike; may be disabled later only with evidence that Loupe-required identity and color are unchanged |
| Materials | Include definition when it does not create separate STEP products |
| Material as separate STEP product | Off |
| Validation properties | On where supported |
| Hidden components | Included |
| Suppressed/excluded components | Omitted |
| PMI and annotations | Off |
| Parameters/custom properties | Off in v1 |
| Datums and construction geometry | Off |
| Cross-sections and cables | Off |
| External references | Forbidden |

The adapter MUST own these settings for the duration of one transfer and MUST
leave the user's persistent export preferences unchanged.

---

## Handoff Contract

### Invocation

```powershell
Loupe.exe --handoff "C:\Users\<user>\AppData\Local\Loupe\Handoffs\<uuid>"
```

Requirements:

- `--handoff` accepts exactly one local directory.
- Relative paths, UNC paths, URLs, and device paths are rejected in v1.
- The directory MUST resolve beneath
  `%LOCALAPPDATA%\Loupe\Handoffs`.
- Unknown manifest fields are tolerated only inside `extensions`.
- Unknown schema major versions are rejected.
- Normal file-open behavior remains unchanged when `--handoff` is absent.
- If Loupe is already running, the handoff MUST be forwarded to the existing
  instance or the new instance MUST own it safely. Duplicate import is not
  acceptable.

### Manifest v1

`handoff.json` is UTF-8 JSON. It describes the transfer, not the model.

```json
{
  "schemaVersion": 1,
  "handoffId": "1f269f77-6d55-49c6-9f7f-0903ff5d8918",
  "createdUtc": "2026-07-29T20:15:32.412Z",
  "producer": {
    "id": "loupe.solidworks",
    "adapterVersion": "0.1.0",
    "cadApplication": "SOLIDWORKS",
    "cadVersion": "2026 SP0"
  },
  "source": {
    "documentTitle": "Gearbox Assembly",
    "documentKind": "assembly",
    "activeRepresentation": {
      "kind": "configuration",
      "name": "Prototype"
    },
    "selectionPolicy": "active-representation-hidden-included",
    "suppressedOrExcludedCount": 4,
    "modelUpAxis": "Z"
  },
  "step": {
    "file": "model.step",
    "protocol": "AP214",
    "packaging": "single-file",
    "geometry": "exact-brep",
    "appearancesRequested": true,
    "bytes": 48291736,
    "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
  },
  "launch": {
    "workspace": "export"
  },
  "extensions": {}
}
```

### Field rules

| Field | Rule |
| --- | --- |
| `schemaVersion` | Integer `1` for this contract. |
| `handoffId` | UUID generated before the directory is created; must equal the directory name. |
| `createdUtc` | RFC 3339 UTC timestamp. |
| `producer.id` | Stable lowercase adapter ID: `loupe.solidworks` or `loupe.creo`. |
| `adapterVersion` | Semantic version of the adapter. |
| `documentTitle` | Display title only; no full source path. |
| `documentKind` | `part` or `assembly`. |
| `activeRepresentation.kind` | `configuration`, `simplified-representation`, or `default`. |
| `selectionPolicy` | Must equal `active-representation-hidden-included` in v1. |
| `suppressedOrExcludedCount` | Non-negative count for user feedback; not used to reconstruct the model. |
| `modelUpAxis` | `X`, `Y`, `Z`, or `unknown`; advisory for Loupe's view naming. |
| `step.file` | Must be the basename `model.step`; paths are forbidden. |
| `step.protocol` | Must be `AP214`. |
| `step.packaging` | Must be `single-file`. |
| `step.geometry` | Must be `exact-brep`. |
| `appearancesRequested` | Must be `true`. |
| `bytes` | Exact file length written before launch. |
| `sha256` | Lowercase SHA-256 of the completed STEP file. |
| `launch.workspace` | Must be `export` in v1. |
| `extensions` | Reserved object for backward-compatible producer additions. |

The manifest MUST NOT contain:

- source CAD file contents;
- source CAD absolute path;
- usernames, email addresses, or machine names;
- the full component tree;
- arbitrary commands or executable paths;
- an output destination for Loupe's later split export;
- credentials or PDM connection data.

### Atomic completion

The producer writes:

1. `model.step.partial`;
2. validates that the native exporter succeeded and produced a non-empty file;
3. renames it to `model.step`;
4. computes size and SHA-256;
5. writes `handoff.json.partial`;
6. renames it to `handoff.json`;
7. launches Loupe only after both final names exist.

Loupe treats the final manifest rename as the handoff commit point. It MUST
never attempt to import `.partial` files.

---

## End-to-End Workflow

### Happy path

1. The user activates a part or assembly in CAD.
2. The user clicks **Send to Loupe**.
3. The adapter preflights document type, active representation, save/rebuild
   state, and translator availability.
4. The adapter allocates a new handoff directory.
5. The adapter applies the controlled STEP profile without opening a dialog.
6. The native translator writes a single AP214 file.
7. The adapter restores every touched user/session preference.
8. The adapter completes the manifest atomically.
9. The adapter locates and launches the installed `Loupe.exe`.
10. Loupe validates the directory, schema, size, checksum, and profile claims.
11. Loupe imports `model.step` through `loupe-worker`.
12. Loupe compares imported facts with the handoff expectations.
13. Loupe opens the Export workspace with the imported assembly ready.
14. Loupe records a redacted diagnostic result.
15. Loupe deletes the accepted handoff after the worker owns a complete native
    document and no retry needs the source file.

### User-visible progress

The CAD host SHOULD show non-modal status:

```text
Preparing active configuration…
Creating Loupe transfer…
Opening in Loupe…
```

The command MUST guard against a second click while one transfer is active.
The CAD UI MUST remain responsive where the vendor API permits asynchronous
work, but all CAD API calls MUST respect the host's required UI/apartment
thread.

### Cancellation

- Cancellation before native export starts removes the new handoff directory.
- Cancellation during export uses the vendor-supported cancel path if one
  exists.
- If native export cannot be interrupted safely, the UI says
  **Finishing STEP conversion…** and does not pretend it was cancelled.
- Cancellation after Loupe launches is owned by Loupe.
- A cancelled or failed handoff is never opened automatically.

---

## Loupe Receiver Requirements

### Command-line and single-instance behavior

`src/app/main.cpp` currently constructs `QGuiApplication` and loads QML without
interpreting application arguments. The receiver implementation MUST:

1. parse `--handoff` before the initial open action;
2. validate locally before sending work to the worker;
3. preserve normal launch and file-open behavior;
4. handle a handoff delivered while Loupe is already running;
5. activate the existing window and switch to Export after import;
6. deduplicate by `handoffId`.

A second-instance mechanism is not currently part of the product baseline.
Choosing the Windows mechanism is an implementation decision, but the behavior
above is a product requirement. Acceptable choices include a local named pipe,
`QLocalServer`/`QLocalSocket`, or a Windows activation mechanism. It MUST be
user-session scoped and MUST NOT listen on the network.

### Importer behavior

Loupe already uses `STEPCAFControl_Reader` with name and color modes and retains
exact OpenCascade/XCAF data in its worker. The v1 receiver MUST explicitly
enable and test:

- name mode;
- color mode;
- layer mode only if needed for correct appearance resolution;
- validation-property mode;
- material and product-metadata modes only when their data is surfaced.

Material/metadata reading MUST NOT delay v1 if geometry, names, hierarchy,
units, placements, and colors are complete. It may be implemented behind the
same import result for a later contract version.

### Imported color precedence

The worker's current display resolution already follows:

```text
face surface/general color
    -> occurrence surface/general color
    -> definition surface/general color
    -> Loupe fallback
```

This order becomes a tested contract. The importer MUST expose enough
diagnostic counts to report:

- number of explicit STEP color assignments;
- number of resolved display-color groups;
- number of distinct imported colors;
- number of faces using Loupe fallback.

If `appearancesRequested` is true but the imported STEP contains zero explicit
colors, Loupe shows a non-blocking warning:

> No source color data was found. Geometry is available, but some parts may use
> Loupe's fallback color.

The absence of assigned color in the original CAD model is valid and MUST NOT
block splitting. Color loss in the certification corpus is a release blocker.

### Handoff validation

Loupe blocks import and reports a stable error when:

- the directory is outside the handoff root;
- the manifest is missing, malformed, or unsupported;
- `handoffId` does not match the directory;
- `model.step` is missing, empty, or not a regular file;
- byte count or SHA-256 differs;
- the manifest claims a non-AP214 or multi-file package;
- the STEP reader reports external references;
- no importable B-rep bodies exist;
- import fails or the worker crashes before accepting the document.

Loupe warns but allows import when:

- source color data is absent;
- manifest component counts cannot be compared;
- advisory up-axis is unknown;
- optional validation properties are absent.

### Cleanup ownership

- Before launch, the CAD adapter owns cleanup.
- After successful manifest validation, Loupe owns cleanup.
- Loupe deletes a successful handoff only after a complete worker document is
  ready.
- Failed handoffs MAY be retained for retry for up to 24 hours, without source
  CAD paths or personal metadata.
- On startup and once per day, Loupe removes handoff directories older than
  24 hours.
- Cleanup MUST resolve and re-check every target beneath the exact handoff
  root; links/reparse-point escapes are rejected.

---

## SOLIDWORKS Adapter

### Recommended implementation

- C# COM add-in implementing the SOLIDWORKS add-in contract.
- Ribbon/CommandManager group containing **Send to Loupe**.
- Vendor interop assemblies isolated in the adapter project.
- Native STEP translator invoked through the documented model save/export API.
- Adapter process remains inside SOLIDWORKS; no OpenCascade dependency.

The native API surface and exact method overload MUST be confirmed against the
lowest supported SOLIDWORKS version during the spike. The adapter SHOULD use
the newest non-obsolete save API available across the certified range and MUST
collect both error and warning codes rather than trusting a Boolean return.

### Controlled settings

The adapter MUST set or prove the following for the single operation:

| SOLIDWORKS concept | Loupe value |
| --- | --- |
| STEP application protocol | AP214 |
| Output geometry | Solid/surface geometry |
| Export appearances | On |
| Export face/edge properties | On initially |
| Export 3D curve features | Off |
| Export assembly components as separate STEP files | Off |
| Output coordinate system | Default |
| Selected components only | Off; clear selection before full active-representation export |
| Lightweight components | Resolve before export or fail with actionable guidance |

The adapter snapshots and restores every changed preference in `finally`-style
cleanup, including on exporter failure.

### Document rules

- Reject drawings and unsupported document types.
- Dirty documents MAY be exported without forcing a save; the transfer
  represents the current in-memory active state.
- The command MUST NOT silently rebuild a model in a way that changes results.
  If a rebuild is required, ask the host to rebuild through its supported path
  and report failure.
- Virtual components and envelope components require explicit fixtures.
- Hidden components are expected in the transfer.
- Suppressed components are expected to be absent.
- Lightweight/unresolved components MUST be resolved or reported; placeholder
  geometry is not acceptable.

### Version policy

The first certification target SHOULD cover the current SOLIDWORKS major
release and two previous majors available to the team. Compile against the
lowest supported interop surface unless the spike demonstrates a safer
version-neutral deployment. Exact service packs belong in the certification
matrix, not in the protocol.

### Distribution

- Register the COM add-in per user where possible.
- Add/remove registration through the installer, not a manual registry script.
- Code-sign the assembly and installer.
- Show one vendor-specific log location and one **Copy diagnostic summary**
  action.
- Evaluate SOLIDWORKS Partner Program requirements before public marketplace
  claims; partner enrollment is not required to prove the local technical
  workflow.

---

## Creo Parametric Adapter

### Recommended implementation

- Native C/C++ Creo TOOLKIT application loaded by Creo Parametric.
- Ribbon command named **Send to Loupe**.
- Registration through a packaged `protk.dat`.
- A Loupe-owned STEP export profile shipped with the adapter.
- `ProIntfExportProfileLoad()` followed by
  `ProIntf3DFileWriteWithDefaultProfile()` is the preferred current API path,
  subject to the profile-restoration spike.

PTC documents the older `ProIntf3DFileWrite()` path as planned for future
deprecation. It MAY be retained behind an adapter interface as a temporary
fallback if it is the only way to avoid mutating the user's active profile, but
that fallback requires an explicit architecture decision and version tests.

### Loupe STEP profile

The bundled profile MUST express:

| Creo STEP profile option | Loupe value |
| --- | --- |
| Application protocol | `ap214_is` |
| Export part as | `As Is` |
| Export assembly as | `Single File` |
| Appearances and layers | On |
| Material definition | On initially |
| Material as separate STEP PRODUCT | Off |
| Assembly validation information | On |
| Entity names derived from IDs | On only if user-defined names override generated IDs |
| Annotations/rich content | Off |
| Datums/extended datums | Off |
| Facets | Off |
| Parameters | Off |
| Cross sections | Off |
| Construction bodies | Off |
| Cable surfaces | Off |
| Export solids and quilts as | `As Is` |
| Reference coordinate system | Default |

The application MUST test that loading this profile does not permanently
replace the user's chosen interactive STEP profile. PTC documents that a
profile loaded with `ProIntfExportProfileLoad()` becomes active in the
interactive session as well. The spike MUST establish one of:

1. snapshot and restore the previous profile reliably;
2. isolate the export in a supported way;
3. use a supported explicit-options fallback.

If none is possible, the Creo adapter is not ready for production even if the
STEP file itself is correct.

### Licensing and build gate

PTC documents that:

- Creo TOOLKIT requires a TOOLKIT license for development and testing;
- locked development applications require that license when loaded;
- distributed applications must be unlocked with PTC's `protk_unlock` process
  so end users do not need a TOOLKIT development license.

Before implementation begins on a Windows machine, confirm:

1. a Creo development seat and matching TOOLKIT installation;
2. access to headers, libraries, samples, and `protk_unlock`;
3. permission and process for unlocking distributed binaries;
4. supported Visual Studio toolset for every targeted Creo major release;
5. whether one binary spans targeted majors or separate builds/installers are
   required.

Until proven otherwise, planning MUST assume a separately built and certified
adapter per Creo major release.

### Model rules

- Active simplified representation is authoritative.
- Hidden components are included.
- Excluded components are omitted.
- Suppressed components are omitted.
- The default coordinate system is used.
- Regeneration errors block transfer with a Creo-native diagnostic.
- Family table instances, flexible components, inherited geometry, and
  assembly-level appearance overrides require dedicated fixtures.

---

## Failure Experience

Errors stay in the application where the user can act on them.

| Failure | Owner | User message/action |
| --- | --- | --- |
| No active model | CAD adapter | "Open a part or assembly, then try again." |
| Unsupported drawing document | CAD adapter | "Send to Loupe supports parts and assemblies." |
| Unresolved/lightweight model | CAD adapter | Offer supported resolve/retry path. |
| Regeneration/rebuild failure | CAD adapter | Name the failed CAD operation; do not launch Loupe. |
| STEP translator unavailable | CAD adapter | State required CAD installation/module. |
| Controlled profile could not be applied | CAD adapter | Abort; never export with unknown defaults. |
| Export produced no file | CAD adapter | Keep CAD open; show copyable diagnostic code. |
| Loupe not installed | CAD adapter | Offer **Locate Loupe** and an official install link; never download silently. |
| Unsupported manifest | Loupe | State adapter/Loupe version mismatch. |
| Checksum mismatch | Loupe | Reject and invite one retry from CAD. |
| External STEP references | Loupe | Reject as an invalid package and name the producer. |
| No imported colors | Loupe | Non-blocking fidelity warning. |
| Worker import failure | Loupe | Preserve handoff for 24-hour retry and show redacted diagnostic ID. |

Stable error-code families:

```text
CAD-DOCUMENT-*
CAD-PROFILE-*
CAD-EXPORT-*
HANDOFF-SCHEMA-*
HANDOFF-INTEGRITY-*
LOUPE-IMPORT-*
LOUPE-COLOR-*
INSTALL-*
```

No diagnostic upload occurs without a separate future product decision and
explicit user consent.

---

## Architecture Boundary

```text
SOLIDWORKS process                 Creo Parametric process
┌──────────────────────┐          ┌────────────────────────┐
│ C# COM add-in        │          │ C/C++ TOOLKIT app      │
│ native STEP exporter │          │ native STEP exporter   │
└──────────┬───────────┘          └───────────┬────────────┘
           │                                  │
           └────────── handoff v1 ────────────┘
                              │
                    model.step + handoff.json
                              │
                     Loupe.exe --handoff
                              │
                   ┌──────────▼──────────┐
                   │ Qt application     │
                   │ validates contract │
                   └──────────┬──────────┘
                              │ existing local protocol
                   ┌──────────▼──────────┐
                   │ loupe-worker.exe   │
                   │ OpenCascade XCAF   │
                   │ exact B-rep owner  │
                   └─────────────────────┘
```

Rules:

- Loupe does not link against SOLIDWORKS or Creo SDKs.
- CAD adapters do not link against OpenCascade.
- No vendor COM or TOOLKIT object crosses a process boundary.
- The file handoff is the only shared vendor-neutral boundary.
- The worker remains the only owner of imported exact geometry.
- A CAD-host crash cannot corrupt an already committed handoff.
- A worker crash cannot crash the CAD host.

---

## Security and Privacy

1. Handoffs are accepted only from the current user's local handoff root.
2. The manifest cannot select an executable, URL, network path, or arbitrary
   source file.
3. The receiver rejects symlinks/reparse points that escape the handoff root.
4. SHA-256 guards accidental modification and incomplete writes; it is not a
   claim that CAD geometry is trusted.
5. STEP parsing remains in the existing isolated worker process.
6. The adapter never executes content from the source model.
7. Diagnostics omit full source CAD paths and STEP geometry.
8. Temporary files use current-user-only permissions.
9. Installers and binaries must be signed before external pilot distribution.
10. Automatic adapter updates are out of scope for v1.

---

## Success Metrics

### Product

- Median interaction count from CAD model to Loupe Export: **one click**.
- No file chooser or STEP options dialog in the happy path.
- At least 95% of certified-corpus transfers open in Loupe without intervention.
- At least 90% of pilot users identify the correct colored component without
  toggling visibility or reading a filename.
- Fewer than 2% of pilot transfers require a second attempt for non-model
  reasons.

### Fidelity

- 100% geometry/body-count match on the required corpus.
- 100% assembly occurrence-count and transform match on the required corpus.
- 100% unit match.
- 100% expected source-name match after documented vendor normalization.
- 100% expected color-precedence match on color fixtures.
- Zero external-reference packages accepted.
- Zero suppressed/excluded components present in the output.
- Zero hidden components missing from the output.

### Reliability

- Zero persistent CAD preference changes after success, failure, or cancellation.
- Zero orphaned handoffs older than the cleanup TTL in automated tests.
- Zero duplicate imports from one `handoffId`.
- CAD remains usable after adapter failure.
- Loupe worker failure does not terminate the CAD host.

---

## Required Certification Corpus

Every required fixture exists natively in both CAD hosts and is also retained
as its resulting STEP and expected manifest facts where licensing permits.

| Fixture | What it proves |
| --- | --- |
| Single colored part | Definition/body color and units. |
| Multi-color face part | Face colors override part color. |
| Two instances of one part with different occurrence colors | Instance color overrides and repeated definitions. |
| Nested three-level assembly | Hierarchy and composed transforms. |
| Multi-body part with distinct body colors | Body splitting and color grouping. |
| Active alternate configuration/representation | Only active design state transfers. |
| Hidden component | Hidden is included. |
| Suppressed/excluded component | Suppressed/excluded is omitted. |
| Lightweight/unresolved assembly | Preflight resolves or blocks safely. |
| Inch assembly containing metric parts | Unit and placement correctness. |
| Unicode and Windows-reserved names | Name preservation and safe later output naming. |
| Chiral, non-axis-aligned assembly | Coordinate transforms are not mirrored. |
| Virtual/flexible/family-table component | Vendor-specific non-file-backed component behavior. |
| Large assembly | Time, memory, cancellation, and single-file limits. |
| Source with no assigned colors | Valid fallback and non-blocking warning. |

Expected facts SHOULD be stored independently from the exported STEP so a
translator defect cannot define its own expected result.

---

## Test Strategy

### Contract tests without CAD installed

- Parse valid manifest v1.
- Reject traversal, UNC, URL, symlink/reparse escape, and wrong-root paths.
- Reject missing and `.partial` files.
- Reject checksum and byte-length mismatch.
- Reject unknown major schema.
- Accept forward-compatible fields only inside `extensions`.
- Deduplicate a repeated `handoffId`.
- Clean expired directories and retain active/recent directories.
- Launch directly into Export after a generated fixture imports.

### Loupe importer tests

- AP214 hierarchy, names, placements, and units.
- Face -> occurrence -> definition -> fallback color precedence.
- Distinct color count and fallback count.
- Repeated definitions retain separate occurrence transforms.
- Multi-body nodes remain independently selectable/exportable.
- External reference classification blocks CAD handoff acceptance.
- Worker crash/failure preserves a retryable handoff.

### Adapter unit tests

- Document-type and active-representation preflight.
- Preference/profile snapshot and restoration on every exit path.
- Correct manifest values and atomic rename sequence.
- Loupe discovery and quoted Windows process launch.
- Concurrent-click guard.
- Adapter version and CAD version reporting.
- No full source path in manifest or logs.

### Host integration tests

Run the required certification corpus in each exact certified host/service
pack. For each case:

1. record native expected counts, transforms, units, names, and appearances;
2. click **Send to Loupe**;
3. capture the resulting manifest and STEP hash;
4. assert Loupe's imported snapshot;
5. visually compare color fixtures;
6. close/reopen CAD and verify export preferences are unchanged;
7. repeat after an induced export failure.

### Performance budgets

Performance is measured separately for native STEP creation and Loupe import.
Initial pilot budgets, subject to corpus evidence:

| Model | CAD conversion p50 | Loupe ready p50 | Peak temporary disk |
| --- | ---: | ---: | ---: |
| Part under 50 MB native | <= 5 s | <= 5 s | <= 2x STEP size + 10 MB |
| Assembly under 500 components | <= 20 s | <= 20 s | <= 2x STEP size + 10 MB |
| Large certified assembly | measured, cancellable, no hard UI hang | measured, progress visible | <= 2x STEP size + 10 MB |

No hard timeout is imposed on valid large models. Progress and cancellation
quality are release criteria.

---

## Delivery Gates

### Gate A — Shared handoff receiver

- Manifest schema and generated fixtures committed.
- `Loupe.exe --handoff` validated on Windows.
- Existing-instance delivery/deduplication proven.
- Export workspace opens automatically.
- Integrity, cleanup, and worker-failure tests pass.

**Exit:** a PowerShell-created fixture can drive the complete Loupe experience
without CAD installed.

### Gate B — Color and metadata certification

- Explicit importer modes reviewed.
- Face/occurrence/definition color precedence tests pass.
- Names, hierarchy, transforms, repeated instances, and units pass.
- Import diagnostics report color coverage.
- External-reference STEP is rejected for the integration path.

**Exit:** the shared receiver is trustworthy before vendor SDK complexity is
introduced.

### Gate C — SOLIDWORKS pilot

- Ribbon command and silent AP214 export work.
- Active configuration, hidden, suppressed, lightweight, and multi-body rules
  pass.
- Export settings restore after success, error, and cancellation.
- Current plus agreed prior releases pass the required corpus.
- Signed pilot installer and uninstall path work.

**Exit:** SOLIDWORKS users can complete the one-click flow.

### Gate D — Creo feasibility spike

- TOOLKIT development license and matching SDK verified.
- Minimal unlocked application loads on a non-development test seat.
- Controlled AP214 single-file export succeeds.
- Profile state is restored with evidence.
- Binary/version packaging approach decided.
- Active simplified representation and color overrides pass focused fixtures.

**Exit:** Creo production scope and cost can be committed. Failure to restore
profile state or unlock/distribute legally blocks production scheduling.

### Gate E — Creo pilot

- Full required corpus passes.
- Per-version installers or a proven shared binary are produced.
- Regeneration, family-table, flexible-component, and simplified-representation
  failures are actionable.
- Signed installation and clean removal pass.

**Exit:** Creo users can complete the one-click flow.

### Gate F — External Windows pilot

- Signed Loupe and adapter installers.
- 10 or more real assemblies per host.
- No preference corruption.
- No missing hidden components or included suppressed components.
- Color-fidelity issues classified and fixed or explicitly blocked.
- Support diagnostics are sufficient without collecting source geometry.

**Exit:** ready for general Windows distribution.

---

## Windows Developer Handoff

### Existing repository baseline

At the time of this PRD:

- `master`, `origin/master`, and tag `v0.1.3` point to commit `65588fe`.
- Windows x64 already builds in GitHub Actions with the
  `windows-release` CMake preset.
- The release currently packages a ZIP containing `Loupe.exe`,
  `loupe-worker.exe`, Qt, and OpenCascade runtime dependencies.
- The app entry point does not yet parse `--handoff`.
- STEP import already uses OpenCascade XCAF and retains exact B-rep in the
  worker.
- Imported display color already resolves face, occurrence, definition, then
  fallback.
- A signed Windows installer and adapter registration do not yet exist.

### Windows workstation prerequisites

For shared Loupe development:

- Windows 11 x64;
- Visual Studio 2022 with Desktop development with C++;
- CMake and Ninja;
- Git;
- vcpkg at the commit pinned by the GitHub workflow;
- Qt/OpenCascade installed through the repository vcpkg manifest.

For SOLIDWORKS:

- certified SOLIDWORKS desktop installations;
- SOLIDWORKS API SDK/help and interop assemblies;
- a C# toolchain compatible with the certified hosts;
- permission to register a development add-in.

For Creo:

- each targeted Creo Parametric major release;
- matching Creo TOOLKIT installation;
- active TOOLKIT development license;
- supported Visual Studio toolset;
- access to `protk_unlock`;
- a separate test seat without TOOLKIT development licensing for unlocked-app
  validation.

### Baseline Loupe build

From a Visual Studio developer shell:

```powershell
$env:VCPKG_ROOT = "C:\src\vcpkg"
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
```

The Windows CI workflow is the authority if local dependency setup differs.
Keep the vcpkg buildtrees path short when reproducing CI to avoid Windows path
length failures.

### Proposed repository layout

```text
src/
  app/
    handoff/
      HandoffManifest.*
      HandoffValidator.*
      HandoffInbox.*
  protocol/
    handoff-v1.schema.json
  integrations/
    shared/
      HandoffWriter/
      LoupeLocator/
    solidworks/
      Loupe.SolidWorks.Addin/
    creo/
      loupe_creo/
      profiles/loupe_ap214.dep
packaging/
  windows/
    loupe/
    solidworks/
    creo/
tests/
  handoff/
  fixtures/handoff/
  integrations/
docs/
  product/
```

Vendor projects MAY use their native build systems if required by their SDK,
but their outputs and tests must remain orchestratable from documented
PowerShell entry points. Do not add vendor SDK binaries to the repository.

### First implementation slice

The first Windows developer should implement only the shared receiver:

1. Add a checked-in JSON Schema equivalent to Manifest v1.
2. Add parser and validation tests using generated STEP fixtures.
3. Add `--handoff` parsing to the Qt entry point.
4. Add local existing-instance delivery and `handoffId` deduplication.
5. Route the validated STEP path through the existing import controller.
6. Switch to Export when the worker reports the document ready.
7. Add color coverage to the worker snapshot/diagnostics.
8. Add cleanup ownership and TTL tests.
9. Add a PowerShell smoke script that creates a handoff from a fixture and
   launches an installed/debug Loupe build.
10. Pass Gate A and Gate B before creating either vendor adapter.

The second slice is the SOLIDWORKS adapter. The Creo licensing/profile spike
may run in parallel once the required PTC environment exists, but Creo code
must not shape the shared handoff contract.

---

## Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Vendor translator drops occurrence or face colors | Parts become hard to distinguish; fidelity claim fails | Required color corpus, AP214, explicit appearance settings, release block on regression. |
| STEP file contains external references | Broken transfer on another process/machine; packaging chaos | Force single-file profiles and reject external references in Loupe. |
| CAD user preferences are mutated | Loss of trust and unexpected later exports | Snapshot/restore; failure-path tests; Creo profile restoration is a dedicated gate. |
| Hidden/suppressed semantics differ by vendor | Wrong parts transferred | Native expected-fact fixtures for both states; block release on mismatch. |
| Creo TOOLKIT licensing/unlocking is unavailable | Creo schedule blocked | Resolve in Gate D before production estimate; keep shared receiver and SOLIDWORKS independent. |
| Creo requires per-major binaries | Higher release and QA cost | Assume per-major packaging until disproven; narrow supported matrix. |
| Large single-file STEP is slow | One-click feels stalled | Native progress, cancellation semantics, measured budgets, no premature multi-file fallback. |
| Loupe is already running | Duplicate or lost handoff | User-session local inbox and `handoffId` deduplication. |
| Manifest and STEP disagree | Wrong metadata or incomplete file | Atomic commit, byte length, SHA-256, import fact checks. |
| Temporary files accumulate | Disk/privacy concern | Explicit ownership and 24-hour stale cleanup. |
| Custom properties are requested too early | Manifest duplicates vendor-specific assembly data | Keep v1 manifest operational; evaluate a mapped metadata extension only after core fidelity ships. |
| Unsigned installers/add-ins trigger enterprise blocks | Pilot cannot install | Code signing and clean installer/uninstaller are release gates. |

---

## Deferred Metadata Extension

After v1, a schema v2 MAY add a deliberately small component metadata map for:

- part number;
- description;
- material display name;
- material density;
- configuration/revision label.

It must not be added until there is a tested, vendor-neutral key that
reconciles a manifest component to the imported STEP occurrence. Display name
alone is not a valid key. A future mapping may use an exporter-injected stable
occurrence token if both native translators preserve it without polluting the
user-visible assembly tree.

Until that proof exists, STEP owns model identity and the manifest owns only
handoff identity.

---

## Open Decisions

These do not block Gate A:

1. Exact supported SOLIDWORKS major releases and service packs.
2. Exact supported Creo major releases and whether binaries are per-major.
3. Windows installer technology and whether adapters ship separately or as
   optional features in one bootstrapper.
4. Existing-instance activation mechanism.
5. Whether material name/density is promoted into v1 after importer evidence,
   without changing the manifest assembly model.
6. Maximum tested STEP size and component count for the external pilot.
7. Product behavior when Loupe is not installed: official download page versus
   locate-existing-install only.
8. Whether a future optional **Differentiate parts** display mode synthesizes
   colors when the source has none.

---

## Acceptance Checklist

- [ ] One ribbon button initiates the transfer in each supported CAD host.
- [ ] No STEP dialog or destination chooser appears in the happy path.
- [ ] The active configuration/simplified representation is transferred.
- [ ] Hidden components are present.
- [ ] Suppressed and explicitly excluded components are absent.
- [ ] Exactly one self-contained AP214 STEP file is produced.
- [ ] No external STEP references are accepted.
- [ ] Exact B-rep, hierarchy, names, transforms, instances, bodies, and units
      pass the required corpus.
- [ ] Original part, occurrence, body, and face colors pass the required
      fixtures.
- [ ] Valid source colors are never replaced by synthetic colors by default.
- [ ] `handoff.json` contains no component tree or source CAD path.
- [ ] Handoff writes and validation are atomic.
- [ ] `Loupe.exe --handoff` imports and opens Export automatically.
- [ ] One `handoffId` cannot import twice.
- [ ] CAD export preferences are identical before and after every tested exit
      path.
- [ ] Failed imports remain retryable for the retention window.
- [ ] Stale handoffs are removed safely.
- [ ] CAD and Loupe failures are isolated by the file/process boundary.
- [ ] SOLIDWORKS certification matrix passes.
- [ ] Creo TOOLKIT licensing, unlocking, profile restoration, and version
      packaging gates pass.
- [ ] Windows binaries and installers are signed before external pilot.
- [ ] Fusion 360 and continuous synchronization remain outside v1.

---

## Primary Technical References

- [SOLIDWORKS STEP file behavior and AP214 color support](https://help.solidworks.com/2025/english/SolidWorks/Sldworks/c_Step_Files.htm)
- [SOLIDWORKS STEP export options](https://help.solidworks.com/2024/english/Solidworks/sldworks/HIDD_EXPORT_OPTIONS_STEP.htm)
- [SOLIDWORKS API overview](https://help.solidworks.com/2026/english/api/sldworksapiprogguide/GettingStarted/SolidWorks_API_Getting_Started_Overview.htm)
- [SOLIDWORKS `IModelDocExtension.SaveAs3`](https://help.solidworks.com/2020/English/api/sldworksapi/SolidWorks.Interop.sldworks~SolidWorks.Interop.sldworks.IModelDocExtension~SaveAs3.html)
- [Creo TOOLKIT 3D export APIs](https://support.ptc.com/help/creo_toolkit/protoolkit_pma/r11.0/usascii/creo_toolkit/user_guide/Exporting_3D_Models.html)
- [Creo TOOLKIT licensing and application unlocking](https://support.ptc.com/help/creo_toolkit/protoolkit_pma/r11.0/usascii/creo_toolkit/get_started/Licensing_for_Creo_TOOLKIT.html)
- [Creo STEP export profile options](https://support.ptc.com/help/creo/creo_pma/r12/usascii/data_exchange/interface/step_export_profile_and_config_options.html)
- [OpenCascade `STEPCAFControl_Reader`](https://dev.opencascade.org/doc/refman/html/class_s_t_e_p_c_a_f_control___reader.html)
