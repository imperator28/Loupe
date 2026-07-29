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
return no contours at all, because the rim was the *only* thing being kept.

**Localised further, by elimination.** For the rim to be the only survivor, the rim has to belong
to an "inside" region — and the rim belongs to just one region, the outer one. The explanation
that fits is that **the splitter never cut the canvas at all** on these views: it returns a single
face, the whole canvas, whose interior sample lands inside the part footprint, so it counts as one
inside region and its only edges are the rim. Everything observed follows from that, before and
after the rim filter.

Two candidate causes were checked and **ruled out**, so the next attempt need not repeat them:

- *The footprint oracle being in a different frame than the split regions.* It is not. HLR uses
  `gp_Ax2(origin, viewDirection, up × viewDirection)` and the oracle's transform uses `gp_Ax3`
  with the same three arguments. Same shape is handed to both.
- *The oracle over-reporting containment.* It is a plain point-in-projected-triangle test with no
  bounding-box fallback, so it cannot report inside for a point outside every triangle.

That leaves the splitter input: the projected edges failing to cut the canvas face. The likely
culprit is `BRepLib::BuildCurves3d` on these particular edges — HLR output carries only pcurves,
that call is what repairs them for the 3D boolean, and an edge whose 3D curve was not built is
silently useless to the splitter. **The next step is to count, for these two views, how many edges
survive `BuildCurves3d` and how many faces the splitter returns.** One is expected to be far lower
than the other.

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

## Root cause found: the two headline defects are one defect

**Ruled out: missing 3D curves.** Measured per edge. Every tool edge carries a 3D curve on every
view, the failing ones included — 28 of 28 on Right, 24 of 24 on Left. `BuildCurves3d` works.

That exhausts the silhouette-side hypotheses, and what remains is arithmetic rather than
speculation. On the failing views the splitter receives 28 valid, coplanar edges and returns the
canvas **uncut**. A planar face can only be divided by edges that either close a loop or run
boundary-to-boundary across it; open fragments that do neither leave the face intact.

The audit already recorded that those views produce exactly such fragments: **`PCBA_box` Right in
cut mode gives 0 closed and 4 open contours**, Left gives 1 closed and 4 open.

So the empty silhouette and the unclosed-contour defect are **the same defect**. The silhouette
method structurally requires closed loops to cut its canvas, so it cannot work on any view where
stitching fails to close them. This explains the whole corpus pattern with no further assumption:
`mount` closes on all six views and its silhouette is right on all six; `590662` has open contours
on four views and its silhouette is wrong on exactly those; `PCBA_box` fails on the two views whose
cut contours do not close.

**The fix belongs in edge stitching, not in the silhouette.** All five eliminated hypotheses were
symptom-chasing. The next work is `ShapeAnalysis_FreeBounds::ConnectEdgesToWires` and why it leaves
fragments open on edge-on views of flat bodies — where projected sharp edges coincide with the
outline and vertices see three or more incident edges. Close the contours and both defects resolve
together; the silhouette code needs no change of its own.

**Hypotheses eliminated by measurement, in order:** footprint oracle frame; oracle over-reporting;
region classifier; splitter tolerance (`SetFuzzyValue`, tried and reverted); missing 3D curves.

### Not a tolerance problem — a topology problem

Checked before touching it: stitching already runs at 0.01 mm on the first pass and 0.1 mm on the
second, and the code carries a measured note that being more permissive *cost* closure (79% down to
68%, because one part lost five already-closed loops when open and closed wires were not
partitioned). So there is no tolerance to widen here, and widening it is known to regress.

The real gap is topological. `ConnectEdgesToWires` connects **chains** — it assumes each vertex has
at most two incident edges. An edge-on view of a flat body breaks that assumption: the top face's
sharp edge and the silhouette outline project onto the *same line*, so vertices see three or more
incident edges. A chain connector cannot resolve a branch point; it stops, and the fragment stays
open. That is why the failure is specific to edge-on views of flat bodies and absent everywhere the
projection has simple topology.

Two candidate fixes, both real work rather than parameter changes:

1. **Dedupe coincident and overlapping edges before stitching**, removing the branches at source.
   `appendUniqueEdges` already dedupes exact duplicates by endpoint key; this needs the harder case,
   collinear *overlap* — one edge lying along part of another — which is what a sharp edge coinciding
   with an outline produces.
2. **Replace chain connection with planar-graph loop extraction.** Build the arrangement of projected
   segments, then walk faces by always taking the next edge counter-clockwise. This is the textbook
   answer, it handles branches by construction, and it would make closure independent of stitching
   tolerance entirely.

Option 1 is smaller and fits the existing pipeline. Option 2 is the one that stops this class of bug
recurring. Either needs corpus verification via `drawing-audit`, which now fails on both an oversized
and an empty silhouette, plus the closure percentages the writers' tests already track.

### Option 1 tried and reverted: collinear-overlap dedupe

Implemented the smaller of the two designed fixes -- drop a straight edge lying entirely along
another collinear straight edge, keeping the longer, so the branch that stops the chain connector is
removed at source. Curves excluded, since a curve sharing endpoints with a line is not contained in
it.

Measured, and it is a partial improvement that misses the target:

- **Cut closure improved on the failing part.** `PCBA_box` Right went from 0 closed contours to 1,
  Left from 1 to 3. So collinear overlap *is* part of the closure problem, confirming the diagnosis.
- **The silhouette did not change.** Right and Left still return one region and produce nothing. The
  remaining open fragments are still enough to leave the canvas uncut.
- **It perturbed another part substantially and unaccountably.** `590662` Front went from 9 split
  regions to 434, Back from 3 to 108, while its silhouette output barely moved. A tenfold change in
  region count with no corresponding change in result is not understood, and shipping it on the
  strength of "the numbers moved" is how the earlier wrong fixes in this session happened.

Reverted on that basis: partial gain, target unfixed, large unexplained side effect.

What it does establish is that the diagnosis is sound and **option 1 is insufficient on its own**.
Removing contained duplicates is not enough because coincident geometry also produces *crossing* and
*T-junction* branches that are not containment cases. That is the argument for option 2 -- planar-graph
loop extraction handles all branch topologies uniformly, rather than enumerating the ones worth
special-casing.
