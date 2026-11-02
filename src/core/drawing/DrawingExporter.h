#pragma once

#include "core/drawing/DrawingFormat.h"
#include "core/drawing/DrawingPlan.h"
#include "core/drawing/DrawingProjector.h"
#include "core/import/StepImporter.h"

#include <filesystem>
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

// A drawing of a shape that is already placed and in millimetres: an exploded assembly,
// say, which no single node of the document stands for.
struct ShapeDrawingRequest final {
    std::filesystem::path destination;
    DrawingFormat format{DrawingFormat::Dxf};
    gp_Dir viewDirection{0.0, 0.0, 1.0};
    gp_Dir upDirection{0.0, 1.0, 0.0};
    ContentMode mode{ContentMode::TechnicalView};
    bool includeScaleFiducial{false};
};

class DrawingExporter {
public:
    // Throws on any failure, leaving no partial file.
    [[nodiscard]] DrawingExportResult writeShape(const TopoDS_Shape& shapeMm, const ShapeDrawingRequest& request) const;

    // Throws on any failure. The caller deletes the partial file and reports the row as
    // failed; a failed row must not abort the batch.
    [[nodiscard]] DrawingExportResult write(const import::ImportResult& imported,
                                            const DrawingOutputRow& row,
                                            bool includeScaleFiducial) const;

private:
    [[nodiscard]] DrawingExportResult projectAndWrite(const ProjectionRequest& request,
                                                      const std::filesystem::path& destination,
                                                      DrawingFormat format, bool includeScaleFiducial) const;
};

} // namespace loupe::drawing
