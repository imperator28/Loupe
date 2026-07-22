# Loupe README and App Icon Design

**Status:** Approved concept
**Date:** 2026-07-19

## Objective

Give the repository a clear GitHub entry point and give the desktop application a recognizable brand icon. The result must accurately present the current functional review build without implying that Loupe is a signed or production-ready release.

## App icon: Layered Glass

The current icon uses the user-supplied layered-glass artwork inside a macOS-style rounded-square container. This supersedes the original magnifying-glass concept.

### Geometry

- Source format: adapted from the supplied self-contained SVG with a `500 x 500` view box.
- The supplied embedded image and vector gradient overlays remain unchanged inside the icon boundary.
- Container: 444-unit rounded square with a 28-unit transparent optical margin and 105-unit corner radius.
- Three translucent elliptical glass layers remain centered and vertically stacked.
- No text or additional decorative elements are introduced.

### Color

- Background: black-to-indigo gradient from upper left to lower right.
- Glass layers: translucent gray-to-lilac gradients with light defining outlines.
- The supplied colors and layer opacity are preserved.
- The design remains recognizable when converted to grayscale.

### Assets and integration

- Keep the editable vector source under `assets/branding/loupe-app-icon.svg`.
- Generate the macOS `.icns` bundle asset from that SVG at standard icon sizes; do not hand-edit raster derivatives.
- Include the SVG in Qt resources and use it as the application/window icon on supported platforms.
- Configure the macOS bundle icon through CMake.
- Render and inspect at 1024, 128, 32, and 16 px before acceptance.

## README

Create a root `README.md` that leads with the vector icon, product name, and a concise description:

> Loupe is a cross-platform desktop application for inspecting STEP assemblies and selectively exporting validated component files.

The README includes:

1. Current status and release-readiness caveat.
2. Core Inspect and Export capabilities.
3. Supported mouse, trackpad, and keyboard navigation.
4. Apple Silicon prerequisites, configure/build/test commands, and launch command.
5. Windows 11 portability status with a link to the portability documentation rather than unverified setup claims.
6. High-level architecture and source layout.
7. Links to the UI refinement handoff, development documentation, roadmap, and evidence policy.
8. Private-corpus guidance so contributors do not commit proprietary STEP files.

The README will not include false CI badges, invented performance numbers, placeholder screenshots, or claims of signing/notarization.

## Acceptance

- The adapted SVG is valid XML, self-contained, and uses a true rounded clipping path.
- The layered-glass mark remains legible at macOS Dock and Finder sizes.
- The app bundle and window use the new icon.
- Debug build and all configured tests pass.
- The README commands match checked-in CMake presets and scripts.
- Only source-controlled branding derivatives are committed; local corpus and temporary STEP files remain excluded.
