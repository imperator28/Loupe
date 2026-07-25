#include "core/drawing/DrawingWriter.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace loupe::drawing {

DrawingWriteError::DrawingWriteError(const Code code, std::string message)
    : std::runtime_error(std::move(message))
    , code_(code)
{
}

PreparedDrawing prepareForOutput(const Drawing& drawing, const DrawingWriteOptions& options)
{
    if (drawing.empty()) {
        throw DrawingWriteError(DrawingWriteError::Code::EmptyDrawing,
                                "the drawing contains no geometry to write");
    }
    const double margin = std::max(0.0, options.marginMm);

    Drawing prepared = drawing;
    if (options.includeScaleFiducial && options.fiducialLengthMm > 0.0) {
        // Sits below the geometry on its own layer, so it is never mistaken for
        // part of the part and is trivial for an operator to measure.
        const auto extents = prepared.bounds();
        const double y = extents.minY - std::max(margin, 2.0);
        Layer fiducial{defaultLayerName(LayerRole::Reference), LayerRole::Reference, {}};
        fiducial.contours.push_back(Contour{
            {Segment{{extents.minX, y}, {extents.minX + options.fiducialLengthMm, y}}}, false});
        prepared.layers.push_back(std::move(fiducial));
    }

    // Translate only. Any scaling here would defeat the entire feature.
    prepared = prepared.translatedToOrigin(margin);
    const auto extents = prepared.bounds();
    return {std::move(prepared), extents.maxX + margin, extents.maxY + margin};
}

std::string formatNumber(const double value, const int decimals)
{
    const double safe = std::isfinite(value) ? value : 0.0;
    std::string text = std::format("{:.{}f}", safe, std::max(0, decimals));
    if (text.find('.') != std::string::npos) {
        const auto lastKept = text.find_last_not_of('0');
        text.erase(lastKept + 1);
        if (!text.empty() && text.back() == '.') text.pop_back();
    }
    // "-0" is valid in every target format but reads as a defect in a diff, and
    // some older DXF readers dislike it.
    if (text == "-0" || text.empty()) text = "0";
    return text;
}

} // namespace loupe::drawing
