# Loupe Governing Design Language

**Design system:** Kinetic Precision

**Status:** Governing for Loupe product UI from Phase 1 onward

**Adopted:** 2026-07-12
**Source:** `Modern_CAD_UI_Governing_Design_Doc_v0.1.docx`

Kinetic Precision is a dark-first desktop design system for a modern CAD viewer and STEP assembly utility. It combines Shapr3D's adaptive selection model, Plasticity's command efficiency, Apple's continuity and motion discipline, Notion's progressive disclosure, and traditional CAD's rigor around units, status, and errors.

This file is the implementation-facing design authority. Product scope and correctness requirements remain governed by the Loupe PRD and backend evidence gates. The current four-phase Loupe roadmap controls delivery sequencing; the source document's illustrative five-phase design-system roadmap does not replace it.

## Normative hierarchy

When sources disagree, use this order:

1. The Loupe PRD defines product scope and user outcomes.
2. Backend contracts and evidence gates define geometry, unit, export, reliability, and privacy correctness.
3. This file defines visual language, interaction, motion, component, accessibility, and platform behavior.
4. Phase implementation plans define sequencing and verification.
5. Feature code and prototypes implement the above; they do not silently redefine them.

`MUST` and `MUST NOT` are requirements. `SHOULD` is the strong default. `MAY` is optional. Exceptions require an explicit design review and a recorded owner, rationale, affected surfaces, and removal condition.

## Product position

Loupe should feel like a modern precision instrument: calm while idle, fast under repeated use, and expressive only when motion clarifies state or spatial relationships. It serves engineers first. Sleek appearance is the result of discipline rather than decoration.

The personality is:

- Precision: dense enough for engineering work; every unit and state is explicit.
- Kinetic: transitions preserve continuity and make cause and effect visible.
- Adaptive: tools and inspectors respond to the selection and task.
- Calm: persistent chrome is minimal, tonal, and low-noise.
- Fast: direct manipulation, command search, context actions, and keyboard paths coexist.
- Trustworthy: no ambiguous state, hidden unit, silent failure, or decorative work animation.

Use these paired constraints during review:

- Sleek, not glossy.
- Technical, not intimidating.
- Minimal, not empty.
- Animated, not restless.
- Dark, not low-contrast.
- Cross-platform, not generic.

## Non-goals

- Do not recreate a ribbon-heavy legacy CAD shell.
- Do not enlarge every control or conceal engineering information behind mobile-style disclosure.
- Do not create a sci-fi dashboard with ambient glow, animated backgrounds, particles, or ornamental data.
- Do not make translucency or blur the default treatment over the viewport.
- Do not make core commands discoverable only through shortcuts.
- Do not animate every change.
- Do not imply geometry editing capabilities that Loupe does not provide.

## Governing principles

1. **Geometry is the hero.** The viewport receives the most space, strongest contrast, and highest input priority. Panels collapse before the model becomes unusably small.
2. **Selection determines relevance.** Available commands and inspector content follow the selected assembly, occurrence, definition, body, face, edge, or measurement.
3. **Motion explains, never delays.** Motion communicates causality, continuity, hierarchy, or spatial change and never blocks the next command.
4. **Progressive disclosure, explicit state.** Secondary actions may appear on hover or selection. Units, warnings, active modes, selection filters, and task state remain visible.
5. **One command, many entry points.** Buttons, menus, palette results, shortcuts, context menus, tests, and automation invoke one stable command identity and handler.
6. **Direct manipulation with numeric authority.** Handles are fast for exploration; precise fields and keyboard entry remain available.
7. **Performance is part of the design.** CAD work runs asynchronously. A spinner never excuses a blocked interface.
8. **Calm default, vivid response.** Neutral tonal layers dominate. Saturated color is reserved for action, selection, warning, error, and transient confirmation.
9. **Reversibility builds trust.** Hide, isolate, ghost, section, and selection changes have obvious exits and recovery.
10. **Platform-native where users notice.** Menus, shortcuts, window behavior, focus, text rendering, file dialogs, and accessibility follow the host platform.

## Reference hierarchy

1. Shapr3D: adaptive, selection-first interaction and canvas dominance.
2. Plasticity: command velocity, minimal viewport chrome, and compact outliner patterns.
3. Apple HIG: continuity, focus, accessibility, and platform behavior.
4. Notion: progressive disclosure, calm hierarchy, and keyboard-accessible structure.
5. NX, Creo, and Rhino: engineering terminology, explicit units, filters, and error rigor.

No reference product should be copied wholesale. In particular, reject touch-sized controls everywhere, persistent glass or blur, document-like whitespace, ribbon overload, modal workflows, and icon-only ambiguity.

## Information hierarchy

| Priority | Layer | Role | Examples |
|---|---|---|---|
| Level 0 | Geometry | Primary content | Model, selection, measurement |
| Level 1 | Task state | Operational context | File, mode, unit, selection count |
| Level 2 | Actions | Available commands | Adaptive strip, floating toolbar, palette |
| Level 3 | Properties | Inspection | Inspector, metadata, validation |
| Level 4 | System | Background state | Progress, diagnostics, notifications |

The model fills the visual field. Panels should feel attached to the work rather than forming a dashboard around it. The interface is nearly motionless while the user reads or manipulates geometry.

## Loupe shell

Loupe retains two product workspaces within one document session:

- **Inspect** is the default and emphasizes tree, viewport, inspection tools, properties, units, warnings, and diagnostics.
- **Export** is a deliberate review workspace with a component list, persistent export checkboxes, master-context preview, isolated preview, naming/grouping controls, and plan validation.

These workspaces are a Loupe-specific application of Kinetic Precision. They do not alter the command, selection, status, or token systems.

### Persistent and transient regions

| Region | Nominal size | Visibility | Governing rule |
|---|---:|---|---|
| Global bar | 40-44 px | Persistent | Cross-context actions, file identity, workspace/mode, command search |
| Assembly tree | 240-340 px | Persistent/collapsible | Virtualized, searchable, status and instance-aware |
| Viewport | Flexible, at least 55% width | Persistent | Maximum remaining space and direct input focus |
| Inspector | 300-380 px | Contextual/collapsible | Selection-driven; stable section ordering |
| View/tool rail | 40-48 px | Optional persistent | View and analysis modes only |
| Status bar | 24-28 px | Persistent | Units, selection, counts, tasks, warnings, diagnostics |
| Command palette | 560-720 px | Transient | Upper-third search for commands, objects, settings, recent files |
| Toast/task HUD | 320-420 px | Transient | Bottom-right; never blocks geometry or critical status |

The Inspect floating toolbar remains bottom-centered and spatially stable. Selection changes availability, labels, or contextual content without moving the toolbar. Active modes transform the toolbar in place and always expose a clear exit.

At widths below 1440 px, the inspector may become an opaque overlay dismissed by Escape. Below 1280 px, the tree defaults to a compact drawer. Density remains constant while layout adapts. The interface must remain functional at 1280 x 720.

## Color tokens

Production QML MUST consume versioned semantic tokens. No production component may contain an unexplained literal color.

| Token | Value | Purpose |
|---|---|---|
| `canvas.0` | `#0B0D10` | Application background and deepest viewport tone |
| `surface.1` | `#11151A` | Primary panels and bars |
| `surface.2` | `#171C23` | Elevated controls and viewport overlays |
| `surface.3` | `#202731` | Popovers, menus, active panel sections |
| `border.subtle` | `#2A323D` | Panel and row separation |
| `border.strong` | `#3A4654` | Focused boundary and draggable splitters |
| `text.primary` | `#F1F4F8` | Primary labels and values |
| `text.secondary` | `#AAB4C0` | Descriptions and inactive labels |
| `text.tertiary` | `#74808E` | Hints and de-emphasized metadata only |
| `action.primary` | `#5B8CFF` | Primary action, active tab, focus ring |
| `selection.geometry` | `#3DD6C6` | Geometry/tree selection semantics only |
| `warning` | `#F2A93B` | Recoverable issue or partial import |
| `error` | `#FF5D68` | Failure, invalid geometry, destructive action |
| `success` | `#4FD18B` | Transient completion or valid check |

Use tonal contrast before shadows. Adjacent surfaces should normally differ by one surface token. Core panels are opaque. Shadows belong only to transient surfaces. Blur or translucency MAY be used for a small overlay only when underlying geometry remains legible.

Color is never the only state cue. Selection adds an edge/marker and slight face tint; warnings and errors pair color with icon and text. Persistent success should neutralize after acknowledgement.

### Viewport treatment

- Background: subtle vertical `#0C1015` to `#131A22` gradient.
- Grid: low contrast, adaptive density, hidden when irrelevant.
- Default bodies: cool neutral gray unless source CAD colors are meaningful.
- Hover: thin action-blue prehighlight without obscuring neighbors.
- Selected: cyan edge plus slight face tint, synchronized with the tree marker.
- Hidden: absent from rendering but still represented in the tree; explicit reveal may ghost it.
- Invalid: error edge or hatch only when diagnostics are active.
- Section: cap material and plane handle use distinct, non-selection colors.

## Typography

Use the platform system sans-serif: Segoe UI Variable on Windows and SF Pro/system font on macOS. Use tabular numerals for dimensions, coordinates, tolerances, and counts. Reserve monospaced text for paths, diagnostics, schema identifiers, and logs.

| Role | Specification | Use |
|---|---|---|
| Display | 24-28 pt, semibold | Start/empty-state title only |
| Title | 15-17 pt, semibold | Document and major panel title |
| Section | 12-13 pt, semibold | Inspector and dialog groups |
| Body | 12-13 pt, regular | Tree rows, labels, menus |
| Caption | 11-12 pt, regular | Secondary metadata and status |
| Numeric | 12-13 pt, medium, tabular | Dimensions and measurements |
| Code | 11-12 pt, mono | Paths and technical diagnostics |

Critical values and units must not use caption styling. Numeric values are right-aligned with units adjacent but visually secondary.

## Density, spacing, and shape

Use a 4 px base increment and an 8 px primary rhythm.

| Token | Value | Use |
|---|---:|---|
| `space.1` | 4 px | Micro gap |
| `space.2` | 8 px | Related controls and row padding |
| `space.3` | 12 px | Panel inset |
| `space.4` | 16 px | Section gap |
| `space.6` | 24 px | Dialog and empty-state separation |
| `space.8` | 32 px | Start/onboarding composition only |
| `row.compact` | 28 px | Tree and dense lists |
| `control.standard` | 32 px | Desktop buttons and fields |
| `toolbar` | 42 px | Global bar; 40-44 px by platform |

| Token | Value | Use |
|---|---:|---|
| `radius.1` | 3 px | Dense fields and tree selection |
| `radius.2` | 5 px | Buttons, menus, compact surfaces |
| `radius.3` | 8 px | Dialog, command palette, large popover |
| `pill` | 999 px | Status tags and segmented indicators only |
| `stroke.icon` | 1.5 px | Custom 20 px outline icon family |
| `divider` | 1 px | Panel/list separation |

Standard buttons must not be pills. Avoid nested cards and excessive rounded containers.

## Iconography

- Use one custom 20 px outline family with 1.5 px stroke and rounded joins.
- Use filled variants only for active state, visibility, or selected mode.
- Unfamiliar CAD operations include a text label on primary and first-use surfaces.
- Do not mix SF Symbols, Fluent, Material, and custom CAD icons in one surface.
- Action icons use verbs; object/type icons use nouns.
- Every icon has a tooltip, accessible name, and shortcut hint where applicable.

## Motion system

Motion is a functional product layer, not visual reward. It confirms input, maintains continuity, clarifies hierarchy, explains spatial change, and communicates progress. Idle UI is still.

| Token | Duration | Use |
|---|---:|---|
| `motion.instant` | 70-90 ms | Press, hover, selection color; no travel |
| `motion.fast` | 110-140 ms | Tooltip, row action, small popover |
| `motion.standard` | 160-200 ms | Inspector, menu, state transition |
| `motion.panel` | 220-260 ms | Panel and command palette |
| `motion.spatial` | 280-360 ms | Fit, isolate, standard camera view |
| `motion.large` | 360-450 ms max | Rare first-load or major workspace shift |

| Token | Curve | Rule |
|---|---|---|
| `ease.enter` | `cubic-bezier(0.16, 1, 0.30, 1)` | Fast response, soft landing |
| `ease.exit` | `cubic-bezier(0.40, 0, 1, 1)` | Exit faster than entry |
| `ease.move` | `cubic-bezier(0.20, 0, 0, 1)` | Relocation without overshoot |
| `ease.linear` | `linear` | Progress/continuous rotation only |
| `spring.controlled` | Critically or near-critically damped | Direct-manipulation release; no visible bounce |

Rules:

- Input acknowledgement starts within one frame.
- Frequent and keyboard-triggered actions use instant or fast timing.
- Prefer opacity and transform; reserve layout animation for meaningful reflow.
- Treat a panel as one animated surface; do not stagger every child.
- Camera motion is interruptible by mouse, touchpad, SpaceMouse, or keyboard.
- Imported geometry appears progressively; parts do not spin or fly into place.
- Move at most two independent UI regions simultaneously unless the whole workspace changes.
- Large lists never receive per-row entrance animation.
- Selection marker and geometry outline update in the same frame when possible.

Reduced-motion mode removes travel, scale, parallax, and spring behavior. Short opacity transitions and immediate state changes remain. Camera fit may use a shorter configurable duration when an instant jump would harm orientation.

## Selection and adaptive actions

Single click selects the most relevant entity for the active selection filter. Repeated click or an explicit cycle command traverses candidates. Shift adds/removes according to platform convention. Tree and viewport selections stay synchronized, while a separate locate action may reveal an object without changing selection.

The inspector summarizes multi-selection and exposes only operations valid for the entire set. If support is partial, the command is disabled with a reason rather than silently omitted or partially executed.

| Selection | Primary actions |
|---|---|
| None | Open, search, view, section, measure mode |
| Assembly | Isolate, hide, fit, export assembly, expand tree |
| Part occurrence | Isolate, hide, fit, export occurrence, locate definition, properties |
| Unique definition | Export once, select all instances, metadata |
| Body/solid | Isolate, hide, mass properties, export, validity |
| Face | Area, type, normal, copy properties, focus |
| Edge | Length, radius/curve type, copy properties |
| Multiple objects | Isolate set, hide set, export selected, compare properties |

Occurrence and unique-definition actions must be visibly distinct and show instance count where relevant.

## Command architecture and keyboard

Every command has:

- a stable identifier;
- localized name, description, and icon;
- selection/mode/document enablement predicate and disabled reason;
- one C++ execution handler;
- platform-aware default shortcut plus user override;
- undoability/view-state classification;
- optional task handle for progress and cancellation.

The command palette uses Cmd+K on macOS and Ctrl+K on Windows. Results include commands, objects, recent files, settings, and help, with scope, shortcut, and disabled reason. Ranking considers exact match, recency, selection, and mode.

Baseline shortcuts:

| Input | Action |
|---|---|
| Escape | Cancel transient action; clear selection only when no action is active |
| Enter | Confirm the active edit or safe default action |
| Space | Temporary navigation modifier or isolate preview; configurable |
| F / Shift+F | Fit selection / fit all |
| H / Shift+H | Hide selected / show all |
| I | Isolate selected |
| M | Measure mode |
| Tab / Shift+Tab | Predictable focus traversal |
| Arrow keys | Tree navigation and numeric adjustment by focused context |

Shortcuts remain visible in menus, tooltips, and the palette. Shortcut-only workflows are prohibited.

## Component governance

Components are specified by behavior and state before styling. Each component documents default, hover, pressed, focused, selected, disabled, loading, error, and reduced-motion behavior where applicable.

### Buttons

- Primary: filled `action.primary`; at most one per panel or dialog.
- Secondary: surface fill plus subtle border.
- Ghost: transparent with hover surface; toolbars and low-emphasis actions.
- Destructive: error text/fill by severity; ordinary Cancel is not red.
- Icon-only: 32 x 32 px with 20 px icon; recognized/repeated actions only; tooltip required.
- Split: only when the default action is stable and safe.

### Assembly tree rows

- 28 px default height; 32 px accessibility density.
- Fixed leading alignment for chevron, type icon, and state marker.
- Middle truncation for generated identifiers; tail truncation for descriptive names.
- Trailing instance badge, visibility, and overflow; low-priority actions appear on hover/selection.
- Selected row uses `surface.3` plus cyan leading marker, not a saturated fill.
- Hidden rows remain selectable with reduced contrast and visibility-off state.
- The model is virtualized; large updates are atomic with no per-row motion.

### Inspector

- Stable collapsible sections by selection type.
- Name, object type, source, validity, and transform precede optional metadata.
- Values align; identifiers and dimensions provide copy affordances.
- Advanced OCCT detail remains collapsed by default.
- Long tasks remain nonmodal in the inspector or task center.

### Fields and values

- Text field: 32 px, visible focus, inline error, persistent label.
- Numeric field: tabular digits and explicit unit suffix; units are never silently inferred.
- Search: clear button and result count; local tree results update within 100 ms after indexing.
- Path: monospaced, browse/reveal actions, middle truncation, full tooltip.
- Read-only values remain selectable and copyable, not disabled-looking.
- Validation appears beside/below the field with icon and text.

### Feedback surfaces

- Toast: short noncritical confirmation, auto-dismiss in 3-5 seconds.
- Banner: document-level warning/degraded mode, persistent until resolved or dismissed.
- Task HUD: expandable progress with cancellation where safe.
- Inline status: preferred for row/field issues.
- Modal alert: only for data loss or unrecoverable decisions.
- Diagnostics center: filterable and linked to affected geometry.

## CAD workflow rules

### Open and import

The shell acknowledges open/drop immediately. Parsing, XDE extraction, validation, tessellation, and GPU upload are asynchronous. Use the fixed progress vocabulary: Reading file, Building assembly, Validating geometry, Generating display, Ready. The tree may become interactive before every mesh completes; progress must not imply false completion.

### Inspection

Selecting a tree row does not move the camera. Fit requires an explicit action; double-click fit remains an interaction-study decision. Properties update immediately, with placeholders only for computed values. Transient measurements disappear on mode exit unless pinned.

### Hide and isolate

Hide is undoable and preserves tree state. Isolate is a visible mode with a persistent chip and clear Exit Isolate action. Ghost Others is explicit and never silently replaces Isolate. Show All does not clear unrelated section or measurement state.

### Section

Section is a visible mode with a clear exit. Direct plane manipulation is paired with numeric position/orientation. Dragging may use a preview or throttled update; exact recomputation occurs on release. The MVP exposes one plane and must not imply an unlimited stack.

### Export

Export remains nonmodal. It explicitly states occurrence versus unique-definition scope, previews names, flags duplicates/collisions/illegal characters, and runs in the background with per-row status. The Run label includes the resolved file count. Validation never silently discards a body. Completion reports exported, skipped, warning, and failed rows with actionable recovery.

### Diagnostics

Errors are object-addressable and written in user language first. Each issue provides summary, affected part/tree path, consequence, expandable technical details, and only technically meaningful recovery actions. Diagnostic export uses a stable schema.

## Platform adaptation

Shared across platforms: semantic tokens, components, icons, viewport interaction, selection, command names, motion timing, tree/inspector/export logic, diagnostic vocabulary, and geometry results.

macOS:

- Native application menu and standard naming.
- Cmd shortcuts, standard windows/tabs, trackpad gestures, smooth scrolling.
- SF Pro/system font and macOS focus/accessibility behavior.
- No Windows-style title-bar controls.

Windows:

- Ctrl shortcuts, Windows file dialogs, system text scaling.
- Precision mouse, touchpad, and optional touch without enlarging desktop density.
- Segoe UI Variable/system font and clear focus rings.
- Custom title bar is optional; native snap layouts and drag behavior remain intact.
- Mica/Acrylic is optional material, never part of the product identity.

A task should be learnable once and recognizable on both platforms. Containers and shortcut modifiers may differ; command meaning, scope, and result do not.

## Qt 6 implementation contract

- Qt Quick/QML owns presentation, tokens, controls, panels, responsive layout, and motion.
- `QAbstractItemModel` and QObject view models expose tree, selection, task, inspector, and command state.
- C++/Qt Core owns document lifecycle, command execution, settings, files, and undoable view state.
- OCCT/XDE owns import, hierarchy, B-rep, validation, tessellation, and export outside the GUI process/thread.
- Rendering remains behind an interface exposing camera, projection, fit, display, selection, highlight, section, and frame timing without leaking OCCT types into QML.
- Heavy CAD work is asynchronous, cancellable, and reports progress.
- Reduced-motion policy is centralized.
- Large trees are virtualized and do not animate delegates during mass updates.
- Nonessential animation pauses or simplifies during orbit, pan, or section manipulation.

Required interfaces remain conceptually stable even if concrete names evolve: viewport controller, selection service, CAD document, task service, command registry, and design preferences.

## Performance and accessibility gates

| Metric | Target |
|---|---:|
| Launch to usable shell | Under 1.5 s on typical modern hardware |
| Input acknowledgement | Under 50 ms target; 100 ms maximum |
| UI main-thread block | Under 8 ms typical; never over 50 ms |
| Tree search | Under 100 ms for 50k indexed nodes |
| Selection synchronization | Same or next frame across viewport/tree/inspector |
| Long-task progress update | 5-10 Hz |
| Motion frame budget | 16.7 ms at 60 Hz; design aware of 8.3 ms at 120 Hz |

Accessibility requirements:

- Support OS text scaling through 200% without clipping core controls.
- Every non-canvas action has an accessible name and role.
- Focus order follows visual order and is visible on every surface.
- Color never carries warning, error, selection, or validity alone.
- Reduced motion and increased contrast are honored where available.
- Tooltips are not the sole source of essential instruction.
- View navigation provides alternatives to middle-button or multi-touch gestures.
- Core workflows are testable with mouse-only, keyboard-heavy, and trackpad input.

## Review gates

| Gate | Required evidence |
|---|---|
| Concept | Workflow map, hierarchy, non-goals; task logic before polish |
| Interaction | Clickable prototype covering states, keyboard, focus, errors, reduced motion |
| Visual | Tokens, density, contrast, and platform variants; no screenshot-only hard coding |
| Technical | Qt/OCCT integration, frame budget, async behavior on target hardware |
| Pre-release | Accessibility, regression, large-file tests, signing and packaging |
| Post-release | Usability findings and measured constraints folded back into this document |

A feature deadline does not justify bypassing tokens, command architecture, accessibility, performance, or asynchronous execution.

## Acceptance checklist

- [ ] Viewport remains dominant while critical state remains visible.
- [ ] Viewport, tree, inspector, and contextual actions agree on selection.
- [ ] Every animation has a purpose, token, and reduced-motion alternative.
- [ ] No CAD task blocks the GUI; key transitions meet frame targets.
- [ ] Core workflows are operable and discoverable by mouse and keyboard.
- [ ] Focus, contrast, names, scaling, and non-color cues are tested.
- [ ] Dark text and boundaries remain legible on multiple display types.
- [ ] Errors identify consequence, affected object, and meaningful next action.
- [ ] Export scope, duplicates, names, warnings, and failures are explicit.
- [ ] Menus, shortcuts, windows, and dialogs fit Windows and macOS conventions.
- [ ] Production UI contains no unexplained literal color, spacing, radius, or duration.
- [ ] Feature-local controls do not duplicate system components.
- [ ] Commands and key states have stable identifiers for testing and support.
- [ ] New components and exceptions are recorded before release.

## Open design decisions

These require explicit future review; implementations must not silently choose:

- Qt Quick-only shell versus hybrid native viewport composition.
- Default viewport navigation preset.
- Selection treatment when source colors are highly saturated.
- Always-visible tree versus distraction-free viewport mode.
- Default diagnostic depth for non-engineering users.
- Minimum supported hardware and assembly size for motion/performance budgets.
- Dark-only first release versus exposed light theme.

The next design proof should exercise one realistic 200-500 part workflow end to end: open, locate, isolate, inspect validity, and export unique parts.

## Frontend design skill stack

Skill discovery selected the following stack for professional Loupe frontend work:

1. **`frontend-design` (Anthropic)** - primary design-direction skill. It has the strongest adoption and source reputation among discovered general frontend-design skills and is already installed.
2. **`qt-ui-design` and `qt-qml` (Qt project skills)** - translate the governing language into native desktop/QML layouts, controls, focus, and bindings.
3. **`web-design-guidelines` (Vercel)** - correctness audit for accessibility, semantics, focus, forms, motion, and interaction behavior in the browser contract.
4. **`design-motion-principles`** - motion creation/audit using restraint appropriate to a high-frequency engineering tool.
5. **`ui-ux-pro-max`** - secondary UX review and polish, never an authority over this token system or product-specific rules.

Do not install the low-adoption generic desktop-design search results unless they later provide a clearly missing capability. The existing verified stack is stronger and more relevant to Qt/QML delivery.
