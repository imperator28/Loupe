#pragma once

#include "core/drawing/DrawingPlan.h"
#include "core/import/StepImporter.h"

#include <string>
#include <vector>

// Turns one reviewed drawing row into one file on disk.
//
// The single place where the four stages meet: resolve the node to a shape, scale it into
// millimetres, project it, and write it. Kept out of the worker so it can be tested
// without a process or a socket, and kept out of the writers so they stay format-only.
namespace loupe::drawing {

struct DrawingExportResult final {
    bool written{};
    double pageWidthMm{};
    double pageHeightMm{};
    int contoursWritten{};
    int openContours{};
    // The exact algorithm failed for this view and a tilted projection was used instead.
    bool approximate{};
    // Warning codes from the projection, e.g. non_solid_bodies_ignored. Carried through so
    // the workspace can say what was approximated rather than reporting a clean success.
    std::vector<std::string> warnings;
};

class DrawingExporter {
public:
    // Throws on any failure. The caller deletes the partial file and reports the row as
    // failed; a failed row must not abort the batch.
    [[nodiscard]] DrawingExportResult write(const import::ImportResult& imported,
                                            const DrawingOutputRow& row,
                                            bool includeScaleFiducial) const;
};

} // namespace loupe::drawing
