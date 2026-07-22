# Loupe Governing Design Language

**Design system:** Indigo Precision (v2)

**Status:** Governing for Loupe product UI

**Adopted:** 2026-07-19
**Supersedes:** Kinetic Precision v0.1 (dark-first, blue/cyan accent)
**Companion authority:** `docs/review/ui-refinement-handoff.md` defines the change boundary (green/yellow/red) and all approved CAD behavior. This document never authorizes a change the handoff forbids.

Indigo Precision is a dual-theme desktop design system for Loupe, a quiet CAD review and selective export instrument. It pairs a restrained slate-neutral shell with a single indigo accent family, follows macOS design language for control details on every platform, and treats motion as a functional layer that exists only to show progress, causality, and input feedback.

## Normative hierarchy

When sources disagree, use this order:

1. The Loupe PRD defines product scope and user outcomes.
2. `docs/review/ui-refinement-handoff.md` defines approved workflows, interaction mappings, and the presentation-only change boundary.
3. Backend contracts and evidence gates define geometry, unit, export, reliability, and privacy correctness.
4. This file defines visual language, tokens, motion, components, accessibility, and platform behavior.
5. Implementation plans define sequencing; feature code implements the above and does not silently redefine it.

`MUST` and `MUST NOT` are requirements. `SHOULD` is the strong default. `MAY` is optional. Exceptions require an explicit design review with a recorded owner, rationale, affected surfaces, and removal condition.

## Product position

Loupe should feel like a modern precision instrument: calm while idle, immediate under repeated use, expressive only when motion clarifies state or spatial relationships. It opens directly into the usable product. Sleek appearance is the result of discipline rather than decoration.

The personality:

- **Precise:** dense enough for engineering work; every unit and state explicit.
- **Quiet:** persistent chrome is tonal, minimal, low-noise; the model is the hero.
- **Indigo:** one saturated accent family, spent deliberately and nowhere else.
- **Native:** control details read as macOS-quality on both platforms.
- **Kinetic where it counts:** progress and cause-and-effect are always visible; idle UI is still.
- **Trustworthy:** no ambiguous state, hidden unit, silent failure, or decorative work animation.

Paired review constraints: sleek not glossy; technical not intimidating; minimal not empty; animated not restless; calm not low-contrast; native not generic.

## Non-goals

- Do not recreate a ribbon-heavy legacy CAD shell or a card-heavy dashboard.
- Do not create a sci-fi surface: no ambient glow, animated backgrounds, particles, or ornamental data.
- Do not make translucency or blur a default treatment; core panels are opaque.
- Do not enlarge controls to touch density or hide engineering data behind mobile-style disclosure.
- Do not make any command discoverable only through shortcuts.
- Do not animate every change, and never animate geometry into place.
- Do not imply editing capabilities Loupe does not provide.

## Governing principles

1. **Geometry is the hero.** The viewport gets the most space, strongest contrast, and highest input priority. Panels collapse before the model becomes unusably small.
2. **Both themes are first-class.** System, Light, and Dark are persisted choices; every token is a measured light/dark pair, including the viewport. Dark values are designed, never inverted.
3. **One accent, spent deliberately.** Indigo appears on primary actions, selection, focus, links, and active states — nothing else. Panels and chrome stay tonal.
4. **Motion explains, never delays.** Every animation shows progress, causality, or input feedback; none blocks or lags input; idle UI is motionless.
5. **Progressive disclosure, explicit state.** Secondary actions may appear on hover or selection, but units, warnings, active modes, and task state stay visible, and hover-only information is always reachable by focus or click.
6. **Direct manipulation with numeric authority.** Handles are fast; exact fields and keyboard entry always coexist with them.
7. **Performance is design.** CAD work is asynchronous; a spinner never excuses a blocked interface; feedback appears within a frame.
8. **State is never color alone.** Selection, hover, validation, and disabled states pair color with edge, icon, weight, or text.
9. **Reversibility builds trust.** Hide, isolate, section, and selection changes have obvious exits; Escape always has a meaning.
10. **Platform-native where users notice.** Menus, shortcuts, dialogs, focus, and text rendering follow the host platform; control detailing follows macOS design language everywhere.

## Theme model

- Persisted choices: **System**, **Light**, **Dark** (handoff §4.2). Theme applies to the whole application including the 3D viewport.
- Theme MUST NOT alter imported STEP colors, assigned material colors, appearance overrides, or captured color meaning.
- Every semantic token below is defined per theme. Production QML consumes only the `Theme` singleton; no production component may contain an unexplained literal color, spacing, radius, or duration.

## Color tokens

All ratios below are measured (`scripts/check_theme_contrast.py`); text roles hold ≥4.5:1 and non-text roles ≥3:1 on every surface they may appear over. Changing any value requires re-running the script.

### Neutrals and text

| Token | Dark | Light | Purpose |
|---|---|---|---|
| `canvas.0` | `#0F1116` | `#F2F3F8` | Window background, deepest layer |
| `surface.1` | `#151823` | `#FFFFFF` | Panels, bars, tree |
| `surface.2` | `#1B1F2C` | `#E9EBF3` | Inset wells, control fills, alternate rows |
| `surface.3` | `#232838` | `#FFFFFF` + shadow | Menus, popovers, dialogs |
| `border.subtle` | `#2A3040` | `#D7DAE6` | Hairline panel/row separation (decorative) |
| `border.panel` | `#4A5266` | `#A8AFC5` | Floating card/panel boundary — calmly visible (~2.2:1), a step above the hairline without functional-boundary weight |
| `border.strong` | `#6C7690` | `#7A829E` | Field outlines, splitters, control boundaries (≥3:1) |
| `text.primary` | `#F2F4F9` | `#1A1D28` | Labels and values |
| `text.secondary` | `#B3BACD` | `#4A5165` | Descriptions, secondary metadata |
| `text.tertiary` | `#8B93A8` | `#616880` | Hints and de-emphasized metadata only; never units or critical values |

The neutral ramp is slate with a slight indigo cast so the accent reads native. Adjacent surfaces differ by one step; tonal contrast before shadows; shadows belong only to transient surfaces (menus, popovers, dialogs, drag ghosts).

### Accent and semantics

| Token | Dark | Light | Purpose |
|---|---|---|---|
| `accent` | `#8B93F8` | `#4F46E5` | Primary action, selection, focus ring, active state, links |
| `accent.vivid` | `#A78BFA` | `#7C3AED` | Section plane/handle and reserved vivid role; never a second action color |
| `on.accent` | `#12142B` | `#FFFFFF` | Text/icon on accent fills |
| `accent.tint` | accent @ 12–16% α | accent @ 10–14% α | Selected row fill, hover wash, checked backgrounds |
| `warning` | `#FDB022` | `#B54708` | Recoverable issue, partial import |
| `error` | `#F97066` | `#B42318` | Failure, invalid input, destructive emphasis |
| `success` | `#47CD89` | `#067647` | Transient completion, valid check |
| `measure` | `#FFD166` | `#8A5300` | Accepted measurement picks and markers (amber role preserved per handoff §5.9) |

Role separation is contractual: viewport **hover** pre-highlight, persistent **selection**, **measurement** amber, export **focus**, and export **checked** are five distinct visual roles and remain simultaneously distinguishable in both themes (handoff interaction gate). Hover and selection derive from `accent` at different strengths plus an edge treatment; measurement stays amber; section manipulators use `accent.vivid` so they never read as selection.

### Viewport treatment

- Dark: near-black background (`canvas.0` family, subtle vertical gradient toward `#161A26` permitted), light defining edges.
- Light: near-white background (`#F7F8FC` family), dark defining edges.
- Default bodies: cool neutral gray unless source CAD colors are meaningful; appearance precedence (override → material → STEP color → fallback) is owned by the handoff and untouched by theme.
- Hover: thin accent pre-highlight that does not obscure neighbors. Selection: accent edge plus slight face tint, synchronized with the tree marker.
- Section cap and plane manipulator use `accent.vivid`; exact 2D slice fills use body colors with theme-aware outlines.

## Typography

Platform system sans-serif: SF Pro (system font) on macOS, Segoe UI Variable on Windows. Tabular numerals for dimensions, coordinates, counts, and offsets. Monospace only for paths, diagnostics, and schema identifiers.

| Role | Specification | Use |
|---|---|---|
| Display | 24–28 pt semibold | Empty-state title only |
| Title | 15–17 pt semibold | Document and workspace titles |
| Section | 12–13 pt semibold | Panel and dialog group headers |
| Body | 12–13 pt regular | Tree rows, labels, menus |
| Caption | 11–12 pt regular | Secondary metadata and status |
| Numeric | 12–13 pt medium, tabular | Dimensions, measurements, offsets |
| Code | 11–12 pt mono | Paths and diagnostics |

Critical values and units never use caption styling. Numeric values right-align with units adjacent but visually secondary.

## Density, spacing, and shape

4 px base increment, 8 px primary rhythm.

| Token | Value | Use |
|---|---:|---|
| `space.1` | 4 px | Micro gap |
| `space.2` | 8 px | Related controls, row padding |
| `space.3` | 12 px | Panel inset |
| `space.4` | 16 px | Section gap |
| `space.6` | 24 px | Dialog and empty-state separation |
| `row.compact` | 28 px | Tree and dense lists |
| `control.standard` | 28–32 px | Buttons, fields, combo boxes |
| `toolbar` | 44 px | Global bar |

| Token | Value | Use |
|---|---:|---|
| `radius.1` | 4 px | Dense fields, tree selection, checkboxes |
| `radius.2` | 6 px | Buttons, segmented controls, inputs |
| `radius.3` | 8 px | Menus, popovers, tool panels |
| `radius.4` | 10 px | Dialogs |
| `pill` | 999 px | Switches, status tags only |
| `divider` | 1 px | Hairlines |
| `focus.ring` | 2 px accent, ~2 px offset | Every focusable control |

Standard buttons are not pills. No nested cards. Hairlines use `border.subtle`; control boundaries (field outlines, splitters) use `border.strong`.

### Panel elevation (light, not outlines)

Panels imply height with light rather than a hard outline, for a seamless, integrated read (the `ElevatedPanel` surface):

- A subtle vertical gradient (`panel.gradient.top` → `panel.gradient.bottom`): dark mode lifts the top toward the light; light mode lets the surface curve gently away toward the bottom.
- A faint top-edge highlight (`panel.highlight`, ~1 px inset by the corner radius) — the lit line that reads as raised.
- A whisper contact hairline (`panel.hairline`: white @ 6% dark / black @ 7% light) instead of the former solid panel border.

| Token | Dark | Light | Purpose |
|---|---|---|---|
| `panel.gradient.top` | `#1B2030` | `#FFFFFF` | Panel fill, top stop |
| `panel.gradient.bottom` | `#141826` | `#F5F7FC` | Panel fill, bottom stop |
| `panel.highlight` | white @ 6% | white @ 70% | Top-edge lit line |
| `panel.hairline` | white @ 6% | black @ 7% | Whisper contact edge |

Content wells (the 3D viewport, standalone preview) skip the gradient and highlight, keeping only the recessed fill plus the hairline. A hard `border.panel`/`border.strong` outline is no longer used for panel cards; it remains only for functional boundaries (fields, the view cube).

## Iconography

- One outline icon family at 16–20 px with 1.5 px stroke and rounded joins; filled variants only for active/selected state.
- Real glyph paths, never text characters (no `⌄` strings as indicators).
- Action icons are verbs; object icons are nouns; unfamiliar CAD operations carry a text label on primary surfaces.
- Every icon-only control has a tooltip, accessible name, and shortcut hint where applicable.
- Do not mix icon families within one surface.

## Motion system

Motion has exactly three sanctioned jobs — **progress**, **causality**, and **input feedback** — and idle UI is still. All motion is transform/opacity; nothing animates layout in hot paths; nothing ever delays or mediates pointer input (handoff §8).

| Token | Duration | Use |
|---|---:|---|
| `motion.instant` | 80 ms | Press, hover, selection color; no travel |
| `motion.fast` | 130 ms | Tooltip, switch thumb, small popover, focus ring |
| `motion.standard` | 180 ms | Menu, state transition, row appear/remove |
| `motion.panel` | 240 ms | Panel collapse, workspace crossfade |
| `motion.spatial` | 320 ms | Fit, isolate, standard-view camera moves |

| Token | Curve | Rule |
|---|---|---|
| `ease.enter` | `cubic-bezier(0.16, 1, 0.30, 1)` | Fast response, soft landing |
| `ease.exit` | `cubic-bezier(0.40, 0, 1, 1)` | Exit faster than entry (~70% of enter time) |
| `ease.move` | `cubic-bezier(0.20, 0, 0, 1)` | Relocation without overshoot |
| `ease.linear` | `linear` | Determinate progress and continuous rotation only |
| `spring.controlled` | Near-critically damped | Direct-manipulation release; no visible bounce |

Rules:

- Input acknowledgement within one frame; frequent and keyboard-triggered actions use instant/fast only.
- Menus/popovers: fade + 2 px rise on enter, plain fade on exit. Panels animate as one surface — no staggered children.
- Workspace switch presents the target immediately (crossfade ≤ `motion.panel`) and populates previews asynchronously.
- Progress: determinate bars animate width via scale transform at `ease.linear`; stage text crossfades; indeterminate shimmer only when truly indeterminate; operations >400 ms show status, >1 s show persistent progress with stage and qualified ETA (handoff §4.3, §8).
- Imported geometry appears progressively; parts never spin or fly in. Large lists never get per-row entrance animation.
- At most two independent UI regions move at once unless the whole workspace changes.
- Camera motion is interruptible by any input.
- **Reduced motion** is a single centralized flag: removes travel, scale, and springs; keeps short opacity fades and immediate state changes; camera fit may use a shortened duration when an instant jump would disorient.

## Control specifications (macOS language)

Every control documents default, hover, pressed, focused, selected, disabled, and (where relevant) error and loading states. All states differ by more than color; all transitions use motion tokens.

### Buttons

- **Primary:** `accent` fill, `on.accent` text, `radius.2`; at most one per panel/dialog. Pressed darkens ~8%; never inverts.
- **Secondary:** `surface.2` fill, `border.strong` 1 px hairline (light mode may drop to `border.subtle` when inside an outlined group), `text.primary`.
- **Ghost:** transparent, hover fills `surface.2`; toolbars and low-emphasis actions.
- **Destructive:** error-colored text (secondary form) or error fill (confirming form); ordinary Cancel is never red.
- **Icon-only:** 28–32 px square, 16–20 px glyph, tooltip required.
- All: hover transition `motion.instant`; visible `focus.ring`; disabled at ~45% opacity plus disabled semantics, never hover-responsive.

### Switch

- macOS proportions: track ~38×22 px pill, 18 px thumb, 2 px inset; no thumb border.
- Off: `surface.2` track with `border.strong` hairline. On: `accent` track, white thumb.
- Thumb slides in `motion.fast` with `ease.move`; state change is instant in reduced motion.

### Checkbox (export inclusion, options)

- 16 px, `radius.1`; unchecked: `surface.1` fill with `border.strong`; checked: `accent` fill with white checkmark drawn in `motion.fast`.
- The export-inclusion checkbox is the only preview-side control that changes the bucket (handoff §6.1); its checked state must remain distinct from row hover and row focus.

### Segmented control (workspace switcher, mode pickers)

- One container at `radius.2`, `surface.2` well; selected segment is a raised `surface.1`/`surface.3` thumb with `text.primary` and a soft shadow in light mode.
- The thumb slides between segments in `motion.standard` with `ease.move`; it is the only moving element.

### Menus and context menus

- `surface.3`, `radius.3`, elevated shadow, 4 px vertical padding, hairline separators.
- Items: 24–26 px rows; highlight is `accent` fill with `on.accent` text (macOS convention); shortcut hints right-aligned in `text.secondary`.
- Content-sized at the cursor, never stretched or sparse (handoff §5.2); enter fade+rise `motion.standard`, exit fade `~70%`.

### Combo box / dropdown

- Field form matches secondary button; real chevron glyph; open state shows `focus.ring`.
- Popup follows menu spec; current item marked with a leading check glyph, not color alone.

### Text and numeric fields

- 28–32 px, `surface.1`/`surface.2` well, `border.strong` outline, `radius.2`; focus swaps outline to `accent` with `focus.ring`.
- Numeric fields use tabular digits and explicit unit suffix; units are never silently inferred.
- Validation appears beside/below the field with icon and text in `error`; inline, on blur, never keystroke-by-keystroke.
- Editing a field suppresses all single-key viewport shortcuts (handoff §5.11).

### Progress

- **Bar:** 4 px track in `surface.2`, `accent` fill, `radius.1` ends; determinate whenever possible; monotonic for import stages.
- **Staged progress (import/refinement):** percentage, current stage label, elapsed, qualified ETA; stage changes crossfade; cancel affordance where the handoff allows; never resembles completion or a freeze (handoff §4.3).
- **Per-row status (export):** icon + text per row (pending, writing, done, failed, canceled) with `success`/`error` pairing; overall bar above the rows.

### Tooltips, toasts, dialogs

- Tooltip: `surface.3`, `radius.2`, caption type, 300–500 ms delay, `motion.fast` fade; never the sole source of essential instruction.
- Toast: bottom-right, noncritical confirmations only, auto-dismiss 3–5 s, never over critical status.
- Dialog: `radius.4`, opaque, `space.6` insets; modal only for data loss or unrecoverable decisions; primary action right-aligned (macOS order).

### Tree and list rows

- 28 px rows; fixed leading alignment for chevron, type icon, and state marker; middle truncation for generated identifiers, tail for descriptive names.
- Hover: `accent.tint` wash (temporary, visually lighter than selection). Selected: `accent.tint` fill plus 2 px accent leading bar — not a saturated full fill.
- Hidden components: reduced contrast plus visibility-off glyph, still selectable.
- Export picker rows show hover, focus (pinned), and checked as three separable treatments: hover = tint wash, focus = accent leading bar + outline, checked = checkbox plus subtle persistent tint.
- Virtualized; atomic mass updates; no per-row motion.

## Shell regions

Loupe opens directly into the product: a global bar, the Inspect workspace (tree + viewport + nonmodal tool panels), and the Export workspace (picker, two previews, bucket, settings). All regions consume the same tokens.

| Region | Nominal size | Rule |
|---|---:|---|
| Global bar | 44 px | File identity, workspace segmented control, units, theme; no decorative branding surface |
| Assembly tree | 240–340 px, collapsible | Virtualized, searchable, status-aware |
| Viewport | Remaining space, ≥55% width | Highest input priority |
| Tool panels | Content-sized, dockable | Movable/collapsible/hidable without disabling active tool results (handoff §5.7) |
| Export desk | Responsive split | Both previews, picker, bucket, settings, status always reachable (handoff §3.1) |

The interface remains functional at 1280×720; below that class of width, panels collapse before the viewport shrinks.

## Platform adaptation

Shared everywhere: tokens, components, icons, control detailing (macOS language), motion, terminology, and results.

- **macOS:** native menu bar and file dialogs, Cmd shortcuts, trackpad gestures, system font, standard window behavior; no Windows-style title-bar controls.
- **Windows:** Ctrl shortcuts, native file dialogs, system text scaling, Segoe UI Variable; native snap/drag behavior intact; Mica/Acrylic never part of product identity.

A task learned on one platform is recognizable on the other; only containers and modifiers differ.

### Cross-platform implementation guard

The "macOS language" above is a **drawn style, not a platform dependency**. To keep every surface portable to Windows:

- All controls are rendered with plain Qt Quick primitives (rectangles, gradients, paths) styled by tokens; no AppKit, Mica/Acrylic, or platform effect APIs may enter production QML.
- Fonts always resolve through the system default (SF Pro on macOS, Segoe UI Variable on Windows); no hard-coded family names, and layouts must tolerate the two fonts' differing metrics without clipping.
- Glyphs used inside controls (chevrons, checkmarks) must render from fonts present on both platforms until the custom icon family lands; verify on Windows before release.
- Native surfaces stay native per platform: menus, file dialogs, title bars, and window management are host-owned and never redrawn to imitate the other platform.
- The focus ring, switch, segmented control, and menu styles are shared verbatim on Windows; only shortcut modifiers, dialog button order, and host chrome differ.
- Windows verification (high-DPI scaling, system text scaling, snap layouts, focus visuals) is part of the pre-release gate, not a port afterthought.

## Qt 6 implementation contract

- The `Theme` QML singleton is the **single source of truth** for every token (color pairs, spacing, radii, type scale, durations, easings, reduced-motion flag). No duplicated palettes, no property-injected theme objects, no literal values in production components.
- QML owns presentation only; controllers own domain state (handoff §7). `QAbstractItemModel`/QObject view models expose state; delegates never own identity or order.
- Heavy CAD work is asynchronous, cancellable, and reports progress; pointer-move paths never trigger expensive CAD work.
- Reduced-motion policy is centralized in the singleton; nonessential animation pauses during orbit, pan, or section drags.
- Large trees are virtualized and never animate delegates during mass updates.

## Performance and accessibility gates

| Metric | Target |
|---|---:|
| Input acknowledgement | <50 ms target, 100 ms max |
| UI main-thread block | <8 ms typical, never >50 ms |
| Feedback for operations >400 ms | Visible status |
| Operations >1 s | Persistent progress with stage/ETA |
| Selection synchronization | Same or next frame across viewport/tree |
| Motion frame budget | 16.7 ms at 60 Hz; aware of 8.3 ms at 120 Hz |

Accessibility:

- OS text scaling through 200% without clipping core controls.
- Every non-canvas action has an accessible name and role; focus order follows visual order; focus is visible on every surface.
- Text ≥4.5:1, non-text controls ≥3:1, verified per theme by `scripts/check_theme_contrast.py`.
- Color never carries state alone; reduced motion honored; Escape always exits transient state without losing unrelated work.
- Mouse-only, keyboard-heavy, and trackpad-only paths all complete core workflows.

## Acceptance checklist

- [ ] Viewport dominant; critical state visible; panels collapse before the model.
- [ ] Both themes complete, including viewport, menus, tooltips, and disabled states; contrast script passes.
- [ ] Accent appears only in sanctioned roles; hover/selection/measure/export-focus/checked remain simultaneously distinct.
- [ ] Every animation maps to a motion token and one of the three sanctioned jobs, with a reduced-motion path.
- [ ] No production literal color, spacing, radius, or duration outside the Theme singleton (grep gate).
- [ ] All controls show macOS-language states including visible focus rings.
- [ ] Import/refinement and export progress can never be mistaken for a hang.
- [ ] Menus content-sized; no clipped or overlapping labels at supported sizes.
- [ ] The 87-test functional baseline and QML smoke tests pass; handoff §10 gates hold.

## Open design decisions

- Exact viewport gradient endpoints per theme (tune against real corpus models).
- Whether the light-theme selected-segment thumb uses shadow or hairline at non-retina DPI.
- Icon family: source set to adopt or draw (must satisfy the one-family rule).
- Command palette: out of current scope; revisit only as a separate product decision.

## Frontend design skill stack

For implementation work: `frontend-design` (direction), Qt project skills (`qt-ui-design`, `qt-qml`) for native QML translation, `design-motion-principles` for motion audits, and `ui-ux-pro-max` for secondary UX review — none of which override this token system or the handoff.
