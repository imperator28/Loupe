#include "core/drawing/DrawingExporter.h"

#include "core/drawing/DrawingProjector.h"
#include "core/drawing/DrawingValidator.h"
#include "core/drawing/DrawingWriter.h"
#include "core/drawing/DxfWriter.h"
#include "core/drawing/PdfWriter.h"
#include "core/drawing/SvgWriter.h"
#include "core/export/ShapeSelection.h"

#include <filesystem>
#include <stdexcept>

namespace loupe::drawing {
namespace {

[[nodiscard]] ContentMode contentModeFor(const DrawingContent content)
{
    switch (content) {
    case DrawingContent::OuterContourOnly: return ContentMode::OuterContourOnly;
    case DrawingContent::CutContours: return ContentMode::CutContours;
    case DrawingContent::TechnicalView: return ContentMode::TechnicalView;
    }
    return ContentMode::CutContours;
}

[[nodiscard]] gp_Dir directionFrom(const Vector3& value, const char* what)
{
    const auto lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (!(lengthSquared > 0.0)) {
        throw std::runtime_error(std::string("reviewed drawing ") + what + " is degenerate");
    }
    return {value.x, value.y, value.z};
}

} // namespace

DrawingExportResult DrawingExporter::write(const import::ImportResult& imported,
                                           const DrawingOutputRow& row,
                                           const bool includeScaleFiducial) const
{
    if (!imported.native || imported.native->document.IsNull()) {
        throw std::runtime_error("import has no native XCAF document");
    }

    const auto& node = exporting::detail::selectedNode(imported, row.nodeId());
    auto shape = exporting::detail::localShape(imported, node);
    // Assembly coordinates always: a drawing of a part in an assembly has to show it as
    // placed, and the view direction the user picked was resolved in assembly space.
    shape = exporting::detail::placedInAssembly(shape, node);

    ProjectionRequest request;
    request.shape = shape;
    request.viewDirection = directionFrom(row.viewDirection(), "view direction");
    request.upDirection = directionFrom(row.upDirection(), "up direction");
    request.mode = contentModeFor(row.content());
    // Two independent factors, and both must be applied to the shape rather than to the
    // projector, which forces its own scale to 1 and discards any baked into its transform.
    // nativeUnitRebase undoes OCCT's normalisation into the XCAF native unit; row.scale()
    // is the drawing scale the user reviewed.
    request.sourceToMillimeters = row.sourceToMillimeters()
        * exporting::detail::nativeUnitRebase(imported) * row.scale();
    request.deflectionMm = 0.01;

    const auto projected = project(request);

    const std::filesystem::path destination(row.finalPath());
    std::filesystem::create_directories(destination.parent_path());
    const DrawingWriteOptions options{2.0, includeScaleFiducial, 50.0};

    DrawingWriteResult written;
    switch (row.format()) {
    case DrawingFormat::Dxf: written = DxfWriter{}.write(projected.drawing, destination, options); break;
    case DrawingFormat::Svg: written = SvgWriter{}.write(projected.drawing, destination, options); break;
    case DrawingFormat::Pdf: written = PdfWriter{}.write(projected.drawing, destination, options); break;
    }
    if (!written.written) throw std::runtime_error("drawing writer reported no output");

    // Read the file back before calling it a success. A writer that miscomputed its page
    // would also miscompute its own report, and a wrong physical size is the one failure
    // that costs material.
    const auto validation = validateDrawing({destination, written.pageWidthMm, written.pageHeightMm, 0.01});
    if (!validation.passed) {
        throw std::runtime_error(validation.errors.empty() ? std::string("drawing validation failed")
                                                           : validation.errors.front().message);
    }

    DrawingExportResult result;
    result.written = true;
    result.pageWidthMm = written.pageWidthMm;
    result.pageHeightMm = written.pageHeightMm;
    result.contoursWritten = written.contoursWritten;
    result.openContours = projected.statistics.openContours;
    result.approximate = projected.approximate;
    for (const auto& warning : projected.drawing.warnings) result.warnings.push_back(warning.code);
    return result;
}

} // namespace loupe::drawing
