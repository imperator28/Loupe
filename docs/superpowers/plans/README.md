# Loupe Implementation Roadmap

The approved design is [Loupe Phase 0 and Product Roadmap Design](../specs/2026-07-10-loupe-phase-0-and-roadmap-design.md).

Execute the plans in order. A later phase may be refined as evidence arrives, but it does not begin until the prior phase's completion gate is reviewed and committed.

| Phase | Goal | Plan | Required prior gate |
|---|---|---|---|
| 0 | Prove backend model/output integrity against both mandatory private corpus files and establish migration readiness | [Phase 0 Feasibility and Contracts](2026-07-10-loupe-phase-0-feasibility.md) | Approved backend-gate design |
| 1 | Validate the UX contract, then deliver the responsive Inspect shell and isolated worker | [Phase 1 Inspection Shell](2026-07-10-loupe-phase-1-inspection-shell.md) | Committed Phase 0 backend evidence |
| 2 | Deliver the dual-view Export workspace and internal pilot | [Phase 2 Selective Export Pilot](2026-07-10-loupe-phase-2-selective-export-pilot.md) | Phase 1 inspection gate |
| 3 | Close regressions, packaging, support, and release evidence | [Phase 3 Hardening and Release Decision](2026-07-10-loupe-phase-3-hardening-release.md) | Phase 2 pilot gate |

Each plan uses test-first changes inside backend/product vertical gates. Focused tests run per change; complete Debug/Release matrices and integrated contract/quality reviews run at gate closure. Logical gate slices are committed before the next gate begins.

Windows 11 is the active platform until the Apple Silicon M2 Pro station joins development. Apple Silicon presets and bootstrap scripts are established in Phase 0. From the first macOS development session onward, Windows and macOS are equally required for every subsequent gate; macOS may be the primary daily-development environment but is never advisory.

Phase 0 is backend-only. The browser interaction prototype begins Phase 1 after backend evidence is committed. Phases 1–3 remain roadmap artifacts until their prerequisites pass.
