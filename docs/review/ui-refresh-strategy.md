# Loupe UI Refresh — Strategic Plan

**Status:** W8 complete for the macOS review build; see `ui-refresh-w8-acceptance.md`
**Date:** 2026-07-19
**Scope authority:** `docs/review/ui-refinement-handoff.md` (change boundary), this document (direction), `design.md` (to be revised as Workstream 1)

## 1. Objective

Refresh Loupe's presentation layer from its functional baseline to a modern, calm, indigo-accented design that follows macOS design language for control details, while changing **zero** CAD behavior, state ownership, or export semantics. All work stays inside the handoff's green zone; anything touching yellow/red is out of scope and flagged, not implemented.

## 2. Current-state assessment

| Area | Finding | Consequence |
|---|---|---|
| Design authority | `design.md` ("Kinetic Precision") is dark-first with blue/cyan accents and defines only dark tokens; the handoff mandates System/Light/Dark parity and the shipped app uses teal + amber | The governing design doc no longer governs; it must be rewritten before implementation |
| Token system | `Theme.qml` holds ~15 colors; `Main.qml` duplicates the whole palette inline; literal hex values appear in every control | No single source of truth; theme changes require shotgun edits; violates design.md's own "no unexplained literal color" rule |
| Non-color tokens | No spacing, radius, typography, elevation, or motion tokens exist anywhere | Density and rhythm are ad hoc; animation cannot be introduced consistently |
| Control kit | `Themed*` controls are flat rectangles: no focus rings on buttons, no state transitions, text-glyph combo indicators, non-native switch proportions | Fails the handoff's accessibility gate (visible focus state) and reads as prototype-grade |
| Motion | Zero animation in the QML layer | Progress, state changes, and workspace switches snap; no feedback vocabulary |
| Shell | Header is a plain ToolBar with an inline-styled ComboBox; workspace switch is a generic TabBar | Weakest first impression in the app; inconsistent with its own control kit |

## 3. Design direction

### 3.1 Theme: Indigo, dual-mode from day one

- **Accent family:** indigo / bright violet. Working palette (to be finalized in the design.md revision with measured contrast):
  - Light mode accent: `#4F46E5` (indigo-600 class) with `#7C3AED` violet reserved for a secondary/vivid role
  - Dark mode accent: `#818CF8` / `#A78BFA` class (desaturated-lightened, per dark-mode practice — never the light accent inverted)
  - Neutrals move from the current blue-gray ramp to a cooler slate ramp tinted very slightly toward indigo, so the accent feels native rather than pasted on
- **Accent discipline (macOS model):** accent appears only on the focused/primary control, selection, links, and active states. Panels, chrome, and rows stay tonal. Saturated color continues to mean something (action, selection, warning, error) — never decoration.
- **Preserved semantic colors:** measurement picks stay amber (handoff §5.9 specifies the persistent amber role); error/warning/success hues are retuned for contrast but keep their meaning. Viewport selection/hover colors may be retuned but the hover / selection / measurement / export-focus roles must remain visually distinct (handoff interaction gate).
- **Both themes are first-class.** Every token is defined as a light/dark pair and contrast-checked independently (≥4.5:1 text, ≥3:1 controls). The viewport participates: near-white viewport with dark defining edges in light, near-black with light edges in dark, per handoff §4.2.

### 3.2 Control language: macOS details, cross-platform tokens

Controls follow macOS design language in geometry and behavior, expressed through our own tokens (not screenshots of AppKit):

- **Buttons:** ~6 px radius, subtle top-lit gradient or flat fill per emphasis level (primary = filled accent, secondary = tonal, ghost = transparent-hover), 1 px hairline border in light mode, pressed = darken not invert, focus ring = 2 px accent halo outside the bounds.
- **Switches:** macOS proportions (pill ~26×15 pt visual scaled to our density), instant thumb slide (~150 ms), accent track when on, no border on the thumb.
- **Menus / popovers:** content-sized, ~8 px radius, elevated shadow, 4 px inset padding, highlight = accent fill with white text, separator hairlines; appear with fast fade+2 px rise, exit faster than enter.
- **Segmented control** replaces the generic TabBar for the Inspect/Export switcher: sliding selection thumb, one moving element.
- **Text fields / spin boxes / combo boxes:** consistent 28–32 px heights, focus ring matching buttons, real chevron glyphs (icon path, not text glyphs).

### 3.3 Motion: progress and causality only

Adopt a small motion token set (durations 70–350 ms, standard enter/exit curves, one controlled-spring) with three sanctioned jobs:

1. **Progress:** staged import/refinement and export progress get animated determinate bars, stage crossfades, and a subtle indeterminate shimmer only when truly indeterminate. Long operations must never look frozen (handoff §4.3, §8).
2. **Causality:** state changes the user triggers (workspace switch, panel collapse, menu open, selection highlight, checkbox → bucket row appearing) animate briefly so cause-and-effect reads.
3. **Feedback:** hover/press color transitions at ~100–150 ms; focus ring fade-in.

Hard rules carried from the handoff: transform/opacity only, nothing delays input, no geometry/layout animation in hot paths, no per-row list entrances, viewport pointer tracking is never mediated by animation, and a centralized reduced-motion switch drops travel/scale.

## 4. Key areas of change (workstreams)

| # | Workstream | Contents | Handoff zone |
|---|---|---|---|
| W1 | **Design authority refresh** | Rewrite `design.md`: indigo dual-theme token tables (color, spacing, radius, type, elevation, motion) with measured contrast, macOS control specs, motion vocabulary, reconciled with the handoff (drop dark-first, drop cyan/blue, keep the governance/quality machinery that still holds) | Docs only |
| W2 | **Token & theme infrastructure** | Make `Theme.qml` the single source: full token set (colors ×2 modes, spacing, radius, type scale, durations, easings); delete the duplicated palette in `Main.qml`; migrate property-injection to the singleton; purge literal colors from all QML | Green |
| W3 | **Control kit modernization** | Rebuild the 11 `Themed*` controls to §3.2 specs + focus rings + state transitions; add missing primitives the workspaces currently improvise (segmented control, progress bar, checkbox, tooltip) | Green |
| W4 | **Shell & chrome** | Header/global bar composition, segmented workspace switcher, restyled theme chooser and unit-review popover, `OpenStepDialog` and empty/loading states | Green |
| W5 | **Inspect workspace polish** | Tree rows (hover/selection/hidden states, density, truncation), dock/panel chrome (Section, Measure, Capture, Context), context menus, viewport overlay controls (projection, standard views), import/refinement progress presentation | Green |
| W6 | **Export workspace polish** | Picker rows (hover/focus/checked distinctness), bucket rows (name+filename hierarchy, grip, insertion indicator), settings forms, validation and per-row export status/progress presentation | Green |
| W7 | **Motion & progress system** | Motion tokens in Theme, reduced-motion switch, staged progress components, workspace/panel/menu transitions per §3.3 | Green (must respect §8 budgets) |
| W8 | **Verification & acceptance** | Full 87-test suite green, QML smoke tests, both-theme contrast audit, literal-color grep gate, before/after captures on corpus files, handoff §10 gate walk | Gate |

Dependency order: **W1 → W2 → W3 → (W4, W5, W6 in parallel) → W7 woven through W4–W6 → W8 continuous, final at the end.**

## 5. Scope boundaries

**In scope:** everything in §4 — presentation, tokens, control styling, layout tuning, transitions, progress display.

**Explicitly out of scope (yellow/red, will not be touched):**
- Any mouse/trackpad/keyboard mapping, selection-state semantics, or hover/focus/checked state merging
- Export enablement rules, plan/worker protocol, naming strategies, coordinate policies
- Import/tessellation/section pipeline; any recomputation-on-pointer-move
- Component identity (stable node IDs), controller-owned state moving into QML
- Measurement amber role removal or viewport recompute behavior

If a refinement turns out to require a yellow-zone change (e.g., a control redesign that would alter when export enables), it is written up for separate review instead of being implemented.

## 6. Success metrics

### Functional (non-negotiable gate)
- All 87 baseline automated tests pass after every workstream; QML smoke tests still cover theme, navigation, pointer routing, section, projection, menus, export loading.
- Corpus STEP files open → preview → refine → inspect → export end-to-end unchanged.

### Design-system integrity (mechanically checkable)
- `grep` for hex literals in `src/app/qml` outside `Theme.qml` returns **zero** production hits.
- Every animation uses a named duration/easing token; a single reduced-motion flag disables travel/scale globally.
- One theme source: the inline palette in `Main.qml` is gone; all controls read the singleton.

### Visual quality
- Every text/background token pair measures ≥4.5:1 and non-text controls ≥3:1 in **both** themes (scripted check against the token table).
- Every interactive control shows a visible focus ring; tab order follows reading order.
- Hover, selection, measurement, export-focus, and export-checked states remain simultaneously distinguishable in both themes (screenshot evidence).
- Menus content-sized, no clipped/overlapping labels at 1280×720 through large desktop sizes.

### Experience
- Import/refinement and export always show staged progress; no state can be mistaken for a hang (manual pass with a large corpus file).
- Micro-interactions complete in ≤200 ms, panel/workspace transitions ≤350 ms, and no interaction waits on an animation (frame capture spot checks against handoff §8 budgets).
- Before/after capture set (shell, tree, section panel, export desk, menus, both themes) reviewed and approved by the user per workstream.

## 7. Approval checkpoints

1. **This document** — direction, workstreams, metrics (now).
2. **W1 output** — revised `design.md` with the concrete indigo token tables and control specs, before any code changes.
3. **Implementation plan** — per-workstream task breakdown with file-level targets, after W1 approval.
4. **Per-workstream visual review** — before/after captures at the end of W3, W4/W5/W6, and W7.
