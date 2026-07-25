# Phase 4 — Gate A: Projection Viability

**Date:** 2026-07-24
**Result:** Passed
**Plan:** [2D Drawing Export](../superpowers/plans/2026-07-24-2d-drawing-export.md)
**Requirements:** [2D drawing export PRD](../product/2026-07-24-2d-drawing-export-prd.md)
**Platform:** Windows 11 x64, MSVC 19.44, `windows-release` preset, OCCT 8.0.0, Qt 6.11.1
**Harness:** `loupe-spike drawing-spike <case> <axis>`

Gate A exists to retire the projection risk before any product code depends on it. Four questions, four measured answers.

## 1. Is orthographic hidden-line removal exactly 1:1?

**Yes — exactly, at 0.0 error.** A 100 × 50 × 10 mm box projected from each standard axis reproduces two of its three dimensions with zero measured deviation, against a 1e-9 tolerance budget.

| View axis | Expected (mm) | Measured (mm) | Max error (mm) |
| --- | --- | --- | --- |
| Z | 50 × 100 | 50 × 100 | 0.0 |
| Y | 10 × 100 | 10 × 100 | 0.0 |
| X | 10 × 50 | 10 × 50 | 0.0 |

This is the feature's central correctness claim and it is now asserted rather than assumed. The projection is a rigid transform with a forced unit scale factor, so any future deviation indicates a bug in our own transform handling, not an inherent approximation. The assertion is retained in the shipping test suite.

## 2. How long does projection take?

**Approximately 1.1–1.2 s** for a 4-body, 171-edge corpus case, measured warm (five prior projections in-process), release build.

This is the gate's least comfortable number and it forced three design decisions:

- Exact projection is **not** viable for a live interactive preview. The candidate preview must debounce, cancel superseded requests, and may use the faster mesh-based algorithm — which the requirements already provide for, on the condition that an approximate preview is labelled as approximate and export always uses the exact path.
- Batch export must report per-drawing progress and remain cancellable. A twelve-drawing batch lands near fifteen seconds, which is acceptable when visible and interruptible.
- Runtime must be re-measured on the largest available assembly before Gate C. If it scales badly with body count, project per body and merge.

## 3. How often does OCCT silently coarsen a curve?

**Zero occurrences** on the corpus case.

OCCT's hidden-line edge builder falls back to a 15-pole, degree-1 B-spline — a 14-segment polyline with no tolerance control — for curve types it cannot classify. On a cut path that is a silent accuracy cliff. It did not fire here, but the detector is cheap and stays in the shipping code so the degradation can never pass unreported.

## 4. Do analytic curves survive projection?

**Circles parallel to the view plane survive exactly.** A cylinder viewed down its own axis yields one analytic circle with an exact diameter.

However, on a real filleted part **139 of 171 edges arrived as B-splines** (109 sharp, 30 outline) with only 32 straight lines and no circles at all. The part's curvature is genuinely non-circular in projection in many places, but not in all of them.

Consequence: the projector gains an **arc-recovery pass** — test each B-spline for constant curvature within tolerance and re-emit it as an arc. Without it, DXF loses its exact `ARC` and polyline-bulge encoding on most real geometry, and output size grows substantially. Recovery rate is to be measured and recorded during Gate B.

## Corrected assumption

The original technical research specified `BRep_Tool::CurveOnPlane` for reading the projected 2D curve. **That is wrong, and it fails silently in the worst way** — it does not read a stored planar pcurve, it *projects the edge's 3D curve* onto the plane supplied. Hidden-line result edges carry no 3D curve, so it returned null for **171 of 171 edges** while reporting success overall.

The correct accessor is the `BRep_Tool::CurveOnSurface` overload that returns the stored pcurve together with its surface and location. Recorded here because the failure mode is a plausible-looking empty result rather than an error, and a reader of the original research would repeat it.

## Constraints recorded for implementation

- `BRepLib::Plane()` — the global plane hidden-line results are expressed on — is a **mutable global static and is not thread-safe.** Projection jobs must be serialised, and the plane must be asserted to be XOY on entry. Verified in the harness.
- Scale the *shape* into millimetres, never the projector: the projector forces its own transform's scale factor to 1 and silently discards a scale baked into it.
- Feed a single pre-located compound. Adding located sub-shapes and hiding them pairwise is quadratic.
- `Prs3d_Projector` was removed in OCCT 8; construct `HLRAlgo_Projector` directly.

## Dependency outcome

No new dependency. `TKHLR`, `TKGeomBase`, `TKShHealing`, and `TKTopAlgo` were already installed and already shipping transitively via `TKXCAF → TKVCAF → TKV3d`; they are now listed explicitly on `loupe-core`. No `vcpkg.json` change and no new packaged library.

## Gate decision

**Proceed to Task 2.** All four questions are answered, the 1:1 claim is proven at zero error, and the one uncomfortable result — runtime — has explicit mitigations rather than open risk.
