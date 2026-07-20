# Export Destination Selection Correction Plan

## Goal

Move the native folder selection into the Export controller as a decoded local path and refresh the visible destination and validation state immediately.

## Tasks

1. Add a `QUrl` destination entry point to `ExportWorkspaceController` that accepts non-empty local-file URLs and delegates storage to the existing path setter.
2. Replace the invalid QML `toLocalFile()` call with the controller URL entry point.
3. Add controller tests for decoded local paths, spaces, rejected remote URLs, and refreshed `canExport` state.
4. Build the app and focused tests, run export and QML smoke suites, refresh the signed macOS review bundle, and relaunch it.

## Acceptance Checks

- Accepting a native folder updates the visible destination field.
- Folder names containing spaces are displayed as decoded local paths.
- Valid bucket rows become ready after destination selection.
- Cancelling or supplying a non-local URL does not alter the previous destination.
