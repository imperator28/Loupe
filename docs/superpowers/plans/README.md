# Loupe Implementation Roadmap

The approved design is [Loupe Phase 0 and Product Roadmap Design](../specs/2026-07-10-loupe-phase-0-and-roadmap-design.md).

Execute the plans in order. A later phase may be refined as evidence arrives, but it does not begin until the prior phase's completion gate is reviewed and committed.

| Phase | Goal | Plan | Required prior gate |
|---|---|---|---|
| 0 | Prove geometry, units, selected export, validation, and UX contracts | [Phase 0 Feasibility and Contracts](2026-07-10-loupe-phase-0-feasibility.md) | Approved design |
| 1 | Deliver the new responsive Inspect shell and isolated worker | [Phase 1 Inspection Shell](2026-07-10-loupe-phase-1-inspection-shell.md) | Phase 0 evidence report |
| 2 | Deliver the dual-view Export workspace and internal pilot | [Phase 2 Selective Export Pilot](2026-07-10-loupe-phase-2-selective-export-pilot.md) | Phase 1 inspection gate |
| 3 | Close regressions, packaging, support, and release evidence | [Phase 3 Hardening and Release Decision](2026-07-10-loupe-phase-3-hardening-release.md) | Phase 2 pilot gate |

Each plan uses test-first steps, small commits, exact paths, and a final evidence gate. Phase 0 is the active plan; Phases 1–3 are roadmap artifacts until their prerequisites pass.
