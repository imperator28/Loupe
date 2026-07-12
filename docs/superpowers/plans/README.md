# Loupe Implementation Roadmap

The approved design is [Loupe Phase 0 and Product Roadmap Design](../specs/2026-07-10-loupe-phase-0-and-roadmap-design.md).

All product UI from Phase 1 onward MUST follow the repository-level [Kinetic Precision governing design language](../../../design.md). The design guide defines the visual tokens, density, motion, command architecture, component behavior, accessibility, platform adaptation, and design review gates. The PRD and backend evidence contracts remain authoritative for product scope and geometry/unit/export correctness.

Execute the plans in order. A later phase may be refined as evidence arrives, but it does not begin until the prior phase's completion gate is reviewed and committed.

| Phase | Goal | Plan | Required prior gate |
|---|---|---|---|
| 0 | Prove backend model/output integrity against both mandatory private corpus files and establish migration readiness | [Phase 0 Feasibility and Contracts](2026-07-10-loupe-phase-0-feasibility.md) | Approved backend-gate design |
| 1 | Validate the UX contract, then deliver the responsive Inspect shell and isolated worker | [Phase 1 Inspection Shell](2026-07-10-loupe-phase-1-inspection-shell.md) | Committed Phase 0 backend evidence |
| 2 | Deliver the dual-view Export workspace and internal pilot | [Phase 2 Selective Export Pilot](2026-07-10-loupe-phase-2-selective-export-pilot.md) | Phase 1 inspection gate |
| 3 | Close regressions, packaging, support, and release evidence | [Phase 3 Hardening and Release Decision](2026-07-10-loupe-phase-3-hardening-release.md) | Phase 2 pilot gate |

Each plan uses test-first changes inside backend/product vertical gates. Focused tests run per change; complete Debug/Release matrices and integrated contract/quality reviews run at gate closure. Logical gate slices are committed before the next gate begins.

Every UI-bearing vertical gate adds five design checks to its existing technical acceptance criteria:

1. Geometry remains visually dominant and selection is consistent across viewport, tree, inspector, and contextual actions.
2. Production UI uses semantic design tokens rather than feature-local color, spacing, radius, or timing literals.
3. Mouse, keyboard, focus, text scaling, contrast, and reduced-motion behavior are verified.
4. CAD work remains asynchronous and UI performance is measured against the Kinetic Precision budgets.
5. Windows and macOS share command meaning and workflow while retaining platform-native menus, shortcuts, focus, windows, and dialogs.

Windows 11 is the active platform until the Apple Silicon M2 Pro station joins development. Apple Silicon presets and bootstrap scripts are established in Phase 0. From the first macOS development session onward, Windows and macOS are equally required for every subsequent gate; macOS may be the primary daily-development environment but is never advisory.

Phase 0 is backend-only. The browser interaction prototype begins Phase 1 after backend evidence is committed. Phases 1–3 remain roadmap artifacts until their prerequisites pass.
