# Drawing workspace UX audit, and agenda for the final review

Audited against `design.md` (the governing design language) and
`docs/review/ui-refinement-handoff.md` (the acceptance gate). Written so the final review has a
list to work through rather than an invitation to look around.

Method: the rules that can be checked mechanically were checked mechanically, because a visual
pass over three workspaces reliably misses exactly the things a grep does not. What a grep
cannot judge is listed separately, unresolved, rather than quietly marked done.

## Checked mechanically — clean

| Rule | Check | Result |
| --- | --- | --- |
| Motion is transform/opacity only; nothing animates layout in hot paths | animations targeting `width`, `height`, `implicitWidth/Height`, `Layout.*` | none found |
| No colour or duration literals in production QML | `qml-style-gates` | passes |
| Token contrast meets WCAG | `qml-theme-contrast` | passes |
| Tracking is size-specific, never one global value | fixed `letterSpacing` in QML | none found, so the new rule is forward-looking rather than a violation |

## Fixed during the audit

**Pan had no bounds at all.** The 2D canvas could be dragged until the drawing left the frame
entirely, with nothing to indicate it was still there. Now the drawing resists progressively
past half a frame and settles back on release, and a drag takes over a settle in flight from
the value on screen rather than fighting it. This is the `spring.controlled` case: the settle is
near-critically damped, because an overshoot would suggest the view bounced off something and
nothing is there.

**Glyph-only buttons had no accessible name.** Fit (`⤢`), the queue reorder arrows and the
remove `×` carried tooltips only, and a tooltip is hover-only — it cannot serve as a name. The
review-queue button also states its count, which lives in a badge and so was absent from the
label a screen reader reads.

**The rubber-band rule itself was too broad** and is now scoped. Resistance needs a continuous
gesture to resist during; stepped input — a wheel notch, an arrow key, a spin box — has no such
gesture and clamps outright. What that owes the user instead is that the limit reads as a limit
and the way back is one action away. The 2D zoom clamp is therefore correct as written, which is
the opposite of what the previous session's note claimed.

## Open, and needs a human

These cannot be settled by inspection. Listed in the order they are worth spending attention on.

1. **Projection reliability, and it dwarfs everything else here.** Silhouette and cut mode are
   still producing wrong geometry: an oversized silhouette bounding box on `PCBA_box` Left and
   Right, up to 80 unclosed contours in cut mode on an edge-on view, and a silhouette that is
   sometimes *smaller* than the cut outline it filters. `drawing-audit` reproduces all three.
   No amount of interface refinement matters while the drawing is wrong.
2. **Whether the 2D preview reads correctly on a real part.** Still the open Gate E item.
3. **Whether pan resistance feels like resistance** rather than like lag. Half a frame of
   allowance is a guess; it wants a hand on a trackpad to confirm.
4. **Empty states.** With no document open the Parts panel is simply blank. The shell's drop
   overlay covers the first-run case, but a closed document leaves an empty panel with no
   guidance. Both pickers share this.
5. **Tab order across the drawing column.** The controls are focusable, but the order through
   preview, setup, and the two queue buttons has not been walked.
6. **Inspect and Export** have not been re-audited since the drawing workspace changed the
   shared `StepViewport` (face-frame picking and the highlight gate) and the shell (a third
   workspace, one shared `workspaces` list). Neither change should have altered them, and the
   `171`-case suite agrees, but "should not have" is not a review.

## Note on scope

The audit covered the Drawing workspace and the shared components it touches. Inspect and
Export were checked only for regression, not reviewed. That is a real gap, not a completed
item, and item 6 above is what remains of it.

## Unresolved: standard-view lag, and what is actually established

Five attempts failed to preserve the zoom on a cube-face click without introducing a visible
lag between the cube turning and the model turning. `setStandardView` is reverted to its
pre-session form: it re-fits, so the zoom resets, but the model turns in step with the cube. A
known imperfect behaviour beats an intermittent one.

**Established, so the next attempt need not re-derive it:**

- The cube renders from `navigation.orientation` through a **binding**, so it turns the instant
  the property changes.
- The main camera rig (`StepViewport.qml`, `id: cameraRig`) has **no bindings** and is written
  **imperatively** by `ViewportNavigation.apply()`.
- The overlay view's camera node has its **own bindings** on the same properties.
- Three sources for one camera state is the shape of the problem.
- A cube rendering as a flat square while the model is oblique proves those sources disagree:
  flat means axis-aligned, oblique means not.
- There are **no camera animations anywhere** in the viewport, so "lag" was never a transition.
- `minimumCoordinate`/`maximumCoordinate` used to scan the whole vertex buffer per call, six
  calls per mesh. Now cached. That was a genuine cost and worth keeping, but it was not this.

**Ruled out:** camera transitions, cube-versus-camera divergence via a second orientation
source, the vertex scan, and ordering alone (both orders were tried, synchronous and deferred).

**The next step is measurement, not another hypothesis.** Drive `setStandardView` from a test
under the offscreen platform and compare, frame by frame, the cube's `cameraOrientation`
against `cameraRig.rotation` and the camera's derived forward vector. That distinguishes "the
camera never received the value" from "it received it a frame late" from "something overwrote
it", which is exactly what guessing could not.

## Resolved: the standard-view lag was the cube hover highlight

Bisected with side-by-side builds after seven failed code-reading diagnoses. The method that
worked: build the commit the user called snappy, build the suspect, compare, then add back one
file at a time. Four measured builds settled what seven readings could not.

| Build | Content | Result |
| --- | --- | --- |
| `a746a83` | before the zoom change | snappy |
| `8dcaac2` | the zoom change | very laggy |
| TEST-A | master, `ViewCube` + `MeshGeometry` rolled back | snappy |
| TEST-B | TEST-A + the cube highlight only | laggy |
| TEST-D | highlight made opaque instead of blended | laggy |
| TEST-E/F | 2D overlay, no model added | snappy, but broke viewport capture |

**The finding, which is not obvious and is worth keeping:** *any* extra `Model` in the view
cube's `View3D` costs measurable frame time — blended or opaque, and whether or not it is
currently `visible`. The cube redraws every frame, so the cost is continuous. It surfaced as
the model appearing to lag the cube on a face click, which sent me hunting through the camera
path for six attempts. Nothing in the camera path was ever wrong.

**Also ruled out by measurement, having been asserted by reasoning:** the number of models
(one was as slow as six), transparency (opaque was as slow as blended), the vertex-extent scan,
`setStandardView` ordering, and camera transitions.

**Shipped:** the hovered face is shown by tinting its existing label, which adds no geometry.

**Rejected, with the reason:** a 2D `Shape` overlay drawing the projected face quad. It was
snappy and it looked right, but it broke `viewportCaptureUsesRequestedRenderResolution` — a 2x
capture came back with no opaque pixels, i.e. it broke the Capture feature. Adding
`QtQuick.Shapes` to the cube changes how that item tree grabs. A face-background tint is still
wanted; it needs a route that does not add geometry to the 3D view and does not disturb capture.

**Reverted:** the eager mesh-extent cache. `upload()` runs on every section rebuild, and the
section's outline width follows the camera, so computing extents there charged every camera
move for a full vertex pass. The accessors are back to scanning per call, which is what the
snappy build did. If this is optimised again it must be lazy, and it must be verified against
capture and camera-move cost, not just the suite.

**Process note.** The suite passed 173/173 through every one of these states, including the
broken ones. It cannot see frame time, and it did not see the capture break until the Shape
went in. Interaction regressions need a human with two builds; that is the cheapest reliable
instrument available here.

## Silhouette: canvas rim leak fixed, region classification still wrong

The oversized silhouette is understood and fixed. The silhouette splitter builds a padded canvas
face, `pad = 10% of the larger projected extent`, and classifies the resulting regions by
sampling a point and asking the mesh oracle whether it is inside material. On an edge-on view of
a flat body the *outer* region's sample reads as inside, so the canvas rim bounds exactly one
"inside" region and survives the boundary count. The arithmetic confirms it rather than suggests
it: `PCBA_box` Right measured 66.42 wide, and `55.35 + 2 x 5.535` is exactly 66.42.

Rim edges are now excluded by comparing against the rim coordinates, which is sound regardless of
the classification, because the pad exists precisely so the rim cannot coincide with real
geometry.

**This did not make those views correct — it made them empty.** `PCBA_box` Left and Right now
return no contours at all, because the rim was the *only* thing being kept. The real defect is
underneath: the region classification fails completely on an edge-on view of a flat body. An
empty drawing is at least honestly wrong rather than plausibly wrong, but it is not a fix.

So `drawing-audit` now also counts an empty silhouette where the cut outline has geometry, and
exits non-zero on it. Without that, this change would have flipped the audit to green while
making two views produce nothing — exactly the false pass the tool exists to prevent.

Current audit state: `mount` clean on all six views; `590662` unchanged; `PCBA_box`
`oversized=0 empty=2`, exit 5.

## The capture test is flaky, and I acted on it as though it were not

`viewportCaptureUsesRequestedRenderResolution` needs real GPU rendering. Measured back to back
with no code changes at all: **failed 45s, failed 36s, passed 6s.** A pass takes about six
seconds; the failures are timeouts.

This matters because earlier in the session I removed a 2D `Shape` overlay -- the projected
face-quad tint, which the user had reviewed and approved as "snappy and clean" -- on the
strength of this test failing three times consistently at 44s. That is the same signature as the
flakiness above. **That removal may have been unnecessary and should be re-tested**, ideally by
running the test several times before and after rather than once.

The general lesson, which cost real work here: a test that is slow when it fails and fast when
it passes is timing out, not detecting. Repeat it before drawing a conclusion from it.
