# GPU Section Preview and Themed Menus Design

## Problem

Section interaction still blocks or trails the pointer because preview updates rebuild sectioned mesh and edge buffers on the UI thread. The fallback slider has a narrow target and jumps to the clicked position, making precise dragging difficult. The section arrow points toward the retained geometry instead of the removed half-space. Native menu delegates also ignore Loupe's dark palette, producing black text on dark backgrounds.

## Goals

- Keep section motion visually attached to the pointer throughout a drag.
- Preserve exact capped and 2D section geometry after interaction ends.
- Make both the in-scene handle and fallback slider easy to acquire and drag.
- Make the arrow point toward the removed half-space and reverse consistently with Flip.
- Make every context and display menu readable in System, Light, and Dark themes.

## Rendering Contract

Section interaction has two explicit phases.

### Preview phase

Pressing the in-scene handle or fallback slider begins preview mode. Models retain their original geometry. A shared GPU clipping-plane uniform controls visible fragments, and pointer motion updates only that plane plus the manipulator position. Preview does not regenerate mesh buffers, CAD edge buffers, caps, or 2D contours.

Only the latest pointer position matters. Preview updates are not queued, and a stale exact result must never overwrite a newer interaction.

### Exact phase

Releasing the pointer commits one final offset and exits preview mode. Loupe then performs the existing exact section rebuild once for the committed offset, including requested caps, 2D fill, and 2D outlines. Exact finalization may take longer than preview but must not change the committed offset or replay intermediate positions.

Canceling an interaction restores its starting offset and does not run exact finalization for an uncommitted position.

## GPU Preview Architecture

The viewport owns one section-preview state containing enabled, plane normal, plane offset, and retained-side sign. Every body material receives the same state. During preview, a clipping shader discards fragments in the removed half-space while preserving each body's imported or overridden color and the current theme lighting.

Normal inspection materials remain active outside preview. Entering and leaving preview switches presentation without replacing source geometry. CAD edge overlays use the same plane state so visible edges agree with visible solids.

If the active GPU backend cannot compile the clipping material, Loupe falls back to a visibly labeled plane-only preview and exact-on-release behavior rather than returning to synchronous per-pointer mesh rebuilding.

## Arrow Convention

The arrow starts on the section plane and its arrowhead points into the removed half-space. Flip reverses the clipping sign, visible arrow direction, and drag direction together. The section plane position remains unchanged when Flip is toggled.

## Slider Interaction

The fallback slider uses a 64-pixel-wide rail and a 30-by-18-pixel handle. It captures the pointer for the duration of the drag.

Dragging is relative to the press position, including when the rail rather than the handle is pressed, so the offset never jumps on acquisition. Shift reduces drag sensitivity to one tenth for fine adjustment. Keyboard arrows and the numeric field remain available for precise edits.

## Themed Menus

Loupe will provide reusable themed menu, menu-item, and separator components. They explicitly define background, normal text, disabled text, highlighted text, selection, and border colors from the active theme.

The assembly-tree context menu, viewport context menus, and display-style menu use these components. The solution does not depend on the operating-system control palette.

## Responsiveness and Feedback

Pointer motion must update the preview without scheduling mesh reconstruction. Exact finalization starts only after release. While exact finalization is active, the committed preview remains visible and a compact non-blocking status indicates that the exact section is being finalized. The viewport remains navigable.

## Verification

Automated coverage will verify:

- Preview state changes do not rebuild section geometry.
- Release produces exactly one committed exact update.
- Cancel restores the starting offset.
- Flip reverses both clip sign and arrow direction.
- Slider drag is relative, bounded, and supports Shift fine adjustment.
- Themed menu delegates expose readable foreground colors in Light and Dark modes.
- Existing exact cap, 2D fill, 2D outline, and section controller tests continue to pass.

Live review will use a large corpus assembly to compare continuous drag responsiveness against exact-finalization time. Dark and Light menu contrast, in-scene handle direction, fallback slider acquisition, caps, and 2D slice output will be checked visually.

## Out of Scope

- Replacing Open Cascade exact section calculations.
- Changing STEP tessellation quality or import refinement policy.
- Redesigning the broader inspection workspace.
