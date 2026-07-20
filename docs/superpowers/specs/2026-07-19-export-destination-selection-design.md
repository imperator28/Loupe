# Export Destination Selection Correction

## Purpose

Ensure that accepting a folder in the native export destination dialog updates the visible destination field and immediately refreshes export validation.

## Design

`FolderDialog.selectedFolder` remains a URL at the QML boundary. QML passes that value directly to a dedicated `ExportWorkspaceController` invokable accepting `QUrl`.

The controller accepts local-file URLs only, converts them with `QUrl::toLocalFile()`, and forwards the resulting native path through the existing destination setter. The setter remains the single place that stores the destination, rebuilds the output plan, and emits the settings notification used by the UI binding.

Manually typed local paths continue to use the existing string destination property. Selecting a folder containing spaces or escaped characters displays its decoded local path rather than a `file://` URL.

If the dialog supplies an empty or non-local URL, the controller leaves the current destination unchanged. Cancelling the dialog has no effect.

## Testing

Focused controller coverage verifies:

- a local folder URL updates the destination to its decoded native path;
- spaces and escaped characters are preserved correctly;
- an empty or non-local URL does not replace the current destination;
- destination changes continue to refresh plan state and `canExport`.

QML smoke coverage verifies that the updated dialog handler loads. The signed macOS review bundle is rebuilt and relaunched for native picker testing.
