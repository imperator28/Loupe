# Loupe README and App Icon Design

**Status:** Approved concept
**Date:** 2026-07-19

## Objective

Give the repository a clear GitHub entry point and give the desktop application a recognizable, original icon based on a physical loupe. The result must accurately present the current functional review build without implying that Loupe is a signed or production-ready release.

## App icon: Precision Glass

The icon uses a macOS-style rounded-square container and a simplified loupe silhouette.

### Geometry

- Source format: hand-authored SVG with a `1024 x 1024` view box.
- Container: rounded square with generous optical margin and approximately 22 percent corner radius.
- Loupe: large circular lens in the upper-left/center area with a short handle angled down-right at approximately 45 degrees.
- The ring and handle form one visually continuous object.
- Shapes remain broad and uncomplicated so the loupe is identifiable at 16 px.
- No text, photographic detail, mesh texture, drop shadow, or thin decorative linework.

### Color

- Background: deep teal-to-cyan diagonal gradient.
- Loupe ring and handle: warm off-white for strong contrast.
- Lens: pale translucent aqua over the background, with one broad flat highlight shape.
- A dark teal connector detail may separate the ring from handle without compromising the silhouette.
- The design must also remain recognizable when converted to grayscale.

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

- The SVG is valid XML and contains only vector geometry/gradients.
- The loupe remains legible at macOS Dock and Finder sizes.
- The app bundle and window use the new icon.
- Debug build and all configured tests pass.
- The README commands match checked-in CMake presets and scripts.
- Only source-controlled branding derivatives are committed; local corpus and temporary STEP files remain excluded.
