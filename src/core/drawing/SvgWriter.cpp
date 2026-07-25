#include "core/drawing/SvgWriter.h"
#include "core/export/AtomicExportFile.h"

#include <cmath>
#include <format>
#include <fstream>
#include <numbers>
#include <string_view>

namespace loupe::drawing {
namespace {

constexpr double pi = std::numbers::pi;

[[nodiscard]] std::string_view strokeForRole(const LayerRole role)
{
    // Cut geometry is black; everything else is visibly secondary so an operator
    // can tell at a glance what is a cut path and what is reference detail.
    switch (role) {
    case LayerRole::Cut: return "#000000";
    case LayerRole::Outline: return "#0000ff";
    case LayerRole::Smooth: return "#808080";
    case LayerRole::Hidden: return "#b0b0b0";
    case LayerRole::Reference: return "#ff00ff";
    }
    return "#000000";
}

[[nodiscard]] std::string point(const gp_Pnt2d& value)
{
    return formatNumber(value.X()) + " " + formatNumber(value.Y());
}

void appendArcSegment(const Arc& arc, std::string& path)
{
    // sweep-flag 1 means SVG's "positive-angle direction". SVG's y axis points
    // down, and by this point the geometry has been mirrored into that same
    // frame, so an increasing angle here is exactly SVG's positive direction --
    // a positive sweep is therefore flag 1.
    const int largeArc = std::abs(arc.sweepAngle) > pi ? 1 : 0;
    const int sweep = arc.sweepAngle > 0.0 ? 1 : 0;
    const auto radius = formatNumber(arc.radius);
    path += std::format(" A {} {} 0 {} {} {}", radius, radius, largeArc, sweep, point(arc.endPoint()));
}

void appendArc(const Arc& arc, std::string& path)
{
    if (!arc.isFullCircle()) {
        appendArcSegment(arc, path);
        return;
    }
    // One elliptical-arc command with coincident endpoints renders nothing at
    // all, so a full circle has to be drawn as two half turns.
    const double half = arc.sweepAngle * 0.5;
    appendArcSegment(Arc{arc.centre, arc.radius, arc.startAngle, half}, path);
    appendArcSegment(Arc{arc.centre, arc.radius, arc.startAngle + half, half}, path);
}

[[nodiscard]] std::string pathData(const Contour& contour)
{
    if (contour.primitives.empty()) return {};
    std::string path = "M " + point(primitiveStart(contour.primitives.front()));
    for (const auto& primitive : contour.primitives) {
        if (const auto* segment = std::get_if<Segment>(&primitive)) {
            path += " L " + point(segment->end);
        } else if (const auto* arc = std::get_if<Arc>(&primitive)) {
            appendArc(*arc, path);
        } else {
            const auto& cubic = std::get<Cubic>(primitive);
            path += " C " + point(cubic.firstControl) + " " + point(cubic.secondControl) + " "
                + point(cubic.end);
        }
    }
    if (contour.closed) path += " Z";
    return path;
}

} // namespace

std::string SvgWriter::serialize(const Drawing& drawing, const DrawingWriteOptions& options,
                                 DrawingWriteResult& result) const
{
    const auto prepared = prepareForOutput(drawing, options);
    // SVG's y axis grows downward while the drawing frame grows upward, so the
    // geometry is mirrored once here. Skipping this produces a vertically
    // mirrored part, which on a chiral outline is scrapped material.
    const auto flipped = prepared.drawing.mirroredVertically(prepared.pageHeightMm);

    const auto width = formatNumber(prepared.pageWidthMm);
    const auto height = formatNumber(prepared.pageHeightMm);

    std::string svg;
    svg += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
    // Physical width/height plus a matching viewBox is what pins one user unit
    // to one millimetre. Emitting only one of the two is how SVG files end up
    // being interpreted at 96 dpi by downstream software.
    svg += std::format("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
                       "width=\"{}mm\" height=\"{}mm\" viewBox=\"0 0 {} {}\">\n",
                       width, height, width, height);
    svg += "<title>Loupe 1:1 drawing</title>\n";

    int contours = 0;
    for (const auto& layer : flipped.layers) {
        const auto name = layer.name.empty() ? defaultLayerName(layer.role) : layer.name;
        svg += std::format("<g id=\"{}\" fill=\"none\" stroke=\"{}\" stroke-width=\"0.1\" "
                           "stroke-linecap=\"round\" stroke-linejoin=\"round\">\n",
                           name, strokeForRole(layer.role));
        for (const auto& contour : layer.contours) {
            const auto data = pathData(contour);
            if (data.empty()) continue;
            svg += "<path d=\"" + data + "\"/>\n";
            ++contours;
        }
        svg += "</g>\n";
    }
    svg += "</svg>\n";

    result.written = false;
    result.pageWidthMm = prepared.pageWidthMm;
    result.pageHeightMm = prepared.pageHeightMm;
    result.contoursWritten = contours;
    return svg;
}

DrawingWriteResult SvgWriter::write(const Drawing& drawing, const std::filesystem::path& destination,
                                    const DrawingWriteOptions& options) const
{
    DrawingWriteResult result;
    const auto svg = serialize(drawing, options, result);

    std::filesystem::create_directories(destination.parent_path());
    exporting::detail::AtomicExportFile partial(destination);
    {
        std::ofstream output(partial.partial(), std::ios::binary);
        if (!output) {
            throw DrawingWriteError(DrawingWriteError::Code::UnwritableDestination,
                                    "unable to open the SVG destination for writing");
        }
        output.write(svg.data(), static_cast<std::streamsize>(svg.size()));
        if (!output) {
            throw DrawingWriteError(DrawingWriteError::Code::EncodingFailed, "failed while writing the SVG");
        }
    }
    partial.finalize();
    result.written = true;
    return result;
}

} // namespace loupe::drawing
