# Landing page v2: plan

Status: draft for review. The prototype lives in `site/v2/` and does not replace `site/index.html` yet.

## Goal

An engineer lands on the page, gets in one screen that Loupe is the fast way to inspect a STEP file, then scrolls through each working scenario played out by the UI itself, and downloads it.

## Headline and voice

Lead (confirmed): **Built by an engineer.**

Second line, under it, choose one:

1. *For the review, not the redraw.* (in the prototype)
2. *The best of NX, SolidWorks, Creo and Fusion. None of the weight.*
3. *Open the file. Understand it. Send only what matters.*

Rotating line under the headline (types in and out, one every ~2.4 s):

> The **fastest STEP viewer** you'll open today. ·
> A **Swiss Army knife** for design review. ·
> **One click** from assembly to every part as its own STEP. ·
> **Deck-ready** renders with a transparent background. ·
> **Cut it open.** Slice it flat. ·
> **Laser-cut outlines** without making a drawing.

Voice: plain, specific, said by someone who has done the job. Every heading names an outcome; the body says how. No "revolutionary", no exclamation marks.

## Page structure

| # | Section | What happens on scroll | Purpose |
|---|---|---|---|
| 1 | **Hero** | Laptop on a dark stage, Loupe running inside it (live, not a screenshot). Headline, rotating line, download buttons for the visitor's platform. | Who it's for, what it is, download now. |
| 2 | **Into the screen** | As you scroll, the laptop grows until its screen fills the window, the lid and base fall away, and the app slides right to make room for the copy column. | The signature moment: from product shot to the instrument itself. |
| 3 | **Workspace stage** (pinned) | Side-by-side. Left: heading and short text per scenario. Right: the Loupe UI, whose tools switch on as you scroll. Under the app, a three-card rail (Inspect / Export / Drawing) shows which workspace you're in. Moving from one workspace to the next slides the whole app sideways, like switching tabs. | Each scenario proved by the UI. |
| 4 | **Built by an engineer** | Short story: what each of NX, SolidWorks, Creo and Fusion does best, and that Loupe keeps that habit. | Why to trust the tool. |
| 5 | **Download and first run** | Platform buttons, unsigned-app first-run steps (carried over from the current page). | Conversion. |

### Scenes in the stage

Vertical scroll moves through the scenes within a workspace; going to the next workspace is a horizontal slide.

**Inspect workspace**

| Scene | Heading (draft) | The UI does | "Pop" element |
|---|---|---|---|
| 0 Speed | Opens before your CAD finishes its splash screen. | Progress bar, model appears part by part, Document properties count up the opening times. | Race bars: Loupe vs SolidWorks vs Creo, **placeholder until benchmarked**. |
| 1 Present | Make it look the way you'll present it. | Auto colours shuffle, a part goes see-through with the opacity slider, background turns transparent, Capture 5x. | A PNG card peels off the viewport with its pixel size. |
| 2 Explode | See how it goes together. | Spread slider drives the explosion, step distance shown. | Dotted travel lines along each part's path. |
| 3 Section | Cut it open. Slice it flat. | Section plane sweeps through, capped faces, then the 2D slice lifts off the model as a flat drawing. | The flat slice floating beside the model. |
| 4 Measure | Measure like you would in CAD. | Hover pre-highlight, then a pinned diameter and distance, ΔX/ΔY/ΔZ panel. | Amber dimension lines drawn in. |
| 5 Mass | Mass, from a material list. | Materials assigned to parts, mass counts up. | Material chips dropping onto parts. |

**Export workspace**

| 6 Split | Every part, its own STEP. One click. | Tree checkboxes tick, the export list fills, files stream out. | A stack of `.step` file chips, one per part. |
| 7 Trim | Strip it down, send the rest. | Hide parts, Export assembly without hidden parts. | Hidden parts drop away, one assembly file remains. |

**Drawing workspace**

| 8 2D | Outlines for the laser and the die cutter. | Top plate projected flat, outline drawn, DXF at 1:1. | The outline draws itself; DXF / SVG / PDF chips. |

The Drawing scene says it is newer and to check drawings before cutting, matching the README limitation.

## Approach and tools

| Need | Choice | Why |
|---|---|---|
| Scroll choreography (pinning, scrubbing, horizontal slide) | **GSAP 3.13 + ScrollTrigger** | Industry standard for launch-page scroll stories, free for commercial use since 3.13, handles pin, scrub, snap and `matchMedia` for mobile and reduced motion. |
| The model in the UI | **three.js** with a model built in code (prototype); a real model exported from Loupe as glTF later | Sharp at any screen density, can be rotated, exploded, sectioned and ghosted live, a fraction of the weight of video. |
| The app around the viewport | HTML/CSS using the app's own tokens from `design.md` | Text stays sharp and selectable, and the tools can light up one by one. |
| Laptop | Drawn in CSS for this site | No third-party device art (licence), scales to any size. |
| Fonts | Archivo (variable width) from Google Fonts | Expanded cuts for display, normal width for body, one family. |
| Smooth scrolling | None (native scroll) | Scroll hijacking hurts trackpads and accessibility. |

Production notes: self-host GSAP, three.js and the font before launch (no third-party CDN at runtime), lazy-start the WebGL scene, and cap device pixel ratio at 2.

### Why live UI rather than screen recordings

Screen recordings would look the most real, but they are heavy, blur at 2x, can't be scrubbed by scroll cleanly, and have to be re-recorded whenever the UI changes. The prototype rebuilds the chrome in HTML and the viewport in WebGL. Real screenshots or short clips still have a place: the scenario gallery and Open Graph image, captured on a Mac at 2x (see below).

## Assets to capture on a Mac

1. A real assembly STEP exported as glTF (or STL per part) to replace the code-built linear stage. Use the same model in every scene.
2. 2x screenshots (Retina, 3456 x 2234 or larger) of each scene, light and dark, for the gallery and social cards.
3. Opening-time numbers from the benchmark below.

## Benchmark protocol (speed scene)

Nothing in the speed scene is published until this is run.

1. **Files:** three public STEP assemblies, small (~50 parts), medium (~500), large (~5,000+). Record file size and part count.
2. **Machine:** one Windows 11 PC for SolidWorks, Creo and Loupe (both CAD programs are Windows-only). Optionally repeat Loupe alone on a Mac. Record CPU, RAM, GPU, drive.
3. **Cold vs warm:** reboot, open once (cold), then close and reopen three times (warm). Report the median of warm and the cold separately. Turn off Loupe's cache for the cold run.
4. **What counts as "open":** from double-click of the file (app already running) to the model fully shown and rotatable. Loupe's own "Ready to view" / "All parts shown" figures are in Document properties and Opening History; for SolidWorks and Creo, time with a screen recording at 60 fps and count frames.
5. **Settings:** default import options; SolidWorks with Large Design Review **off** and **on** (report both, because LDR is SolidWorks' fast-open answer); Creo with default STEP import.
6. **Publish:** times, the files, the machine and the method, on a page linked from the speed scene.

## Responsive and accessibility

- Under 900 px the stage stops pinning side-by-side; the app sits on top (sticky) and the copy scrolls under it.
- `prefers-reduced-motion`: no pinning or scrubbing; each scene is shown as a still with its copy, and the rotating line stops on the first entry.
- WebGL fails: a static screenshot stands in.

## Open questions for review

1. Second line of the headline (options above).
2. Do we keep a separate features page, or does the stage replace it?
3. Which real assembly do we show (licence-clear, looks good exploded and sectioned)?
4. Should the stage snap to each scene, or scrub freely?
