# Product

<!-- impeccable:product-schema 1 -->

## Platform

web (this record covers the website in `site/`; the desktop app's own design language is `design.md`)

## Users

Mechanical and product engineers who receive STEP files from colleagues, vendors and customers and need to review them without opening a full CAD seat: check how an assembly goes together, measure it, cut it open, work out mass, put it in a review deck, and pass on only the parts the next person needs. Many of them run NX, SolidWorks, Creo or Fusion 360 every day and know how slow those programs are for a look-only job.

## Product Purpose

Loupe is a free, open-source desktop STEP inspector for macOS (Apple Silicon and Intel) and Windows 11. It opens STEP files fast, gives review and measurement tools close to a professional CAD program, and exports exactly what the next person needs: separate STEP or STL per part, a trimmed assembly, a transparent high-resolution PNG, or a true-scale 2D drawing.

Success for the website: an engineer understands within one screen that Loupe is the fast way to inspect a STEP file, sees each working scenario proved by the real UI, and downloads it.

## Positioning

Designed by an engineer who has spent years in NX, SolidWorks, Creo and Fusion 360, taking the best habit of each into one fast inspector. It is for reviewing a design, not redrawing it. Two jobs professional CAD makes tedious and Loupe makes one step: writing every component out as its own STEP file, and trimming an assembly by hiding parts and re-exporting.

## Operating Context

- Inputs: `.step` / `.stp` files, opened from the picker, by drag and drop, or by double-click.
- Workspaces: **Inspect** (review, measure, section, explode, materials and mass, saved views, capture), **Export** (split-part STEP/STL export, strip-and-resave), **Drawing** (2D projection to DXF, SVG, PDF at 1:1).
- Outputs go into review decks (transparent PNG at 2x/3x/5x or custom scale), to vendors and prototype shops (per-part STEP/STL), and to laser or die cutters (DXF outlines).

## Capabilities and Constraints

- Section on an axis or a face plane, with capped cut faces and an outline / filled / filled-plus-outline 2D slice.
- Exploded view with Spread, drag handles, exact offsets, saved layouts, and export of the layout.
- Measure: point to point, edge length, surface to surface, radius/diameter, angle, area, volume, bounds; pinned results.
- Materials library and mass for a body, subassembly or selection.
- Transparency with an opacity slider; automatic part-colour palettes.
- Opening time is recorded per file (ready to view, all parts shown, fully detailed) with an Opening History CSV.
- Drawing workspace is newer: projection is not yet production-reliable (Cut mode can leave unclosed contours). Site copy must not call drawings cut-ready.
- Not code-signed yet; first-run install steps must stay on the site.

## Brand Commitments

- Name: Loupe. App icon: `assets/branding/loupe-app-icon.svg`, site icon `site/icon.svg`.
- App visual language: Indigo Precision (`design.md`), indigo accent, amber for measurement.
- Website direction confirmed by the owner (2026-11-06): dark stage; launch-page storytelling by sections; the opening shows the UI inside a laptop drawn for the site in the spirit of a current pro laptop (thin black bezel, camera island, dark base), with no third-party device artwork; the headline leads with "Built by an engineer."
- Do not mention Apple in site copy.

## Evidence on Hand

- Screenshots of the real app, light and dark: `site/img/*.webp` (hero, explore, measure, section, explode, export, drawing). Captured on Linux; macOS captures are still to be made.
- No measured head-to-head opening benchmark against SolidWorks or Creo exists yet. Any speed comparison stays a labelled placeholder until the owner runs the benchmark protocol (`docs/website/landing-v2-plan.md`).
- No testimonials, customer names or download counts. Do not invent them.

## Product Principles

1. Prove each claim with the real UI doing the job, not with adjectives.
2. Speed is the first impression; never fake it with numbers we have not measured.
3. Sell working scenarios (a review deck, a vendor package, a cutter file), not a tool list.
4. Honest about what is new or unfinished.

## Accessibility & Inclusion

Every scroll-driven scene must have a reduced-motion path that shows the same content without pinning or scrubbing, and all copy must stay real text.
