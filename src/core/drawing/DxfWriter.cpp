#include "core/drawing/DxfWriter.h"
#include "core/export/AtomicExportFile.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <numbers>
#include <string_view>
#include <vector>

namespace loupe::drawing {
namespace {

constexpr double pi = std::numbers::pi;
constexpr double twoPi = 2.0 * pi;
// Chordal tolerance for flattening cubics. R12 has no spline entity, so a Bezier
// has to become a polyline; an order of magnitude below laser kerf is invisible.
constexpr double cubicToleranceMm = 0.01;
constexpr int maximumFlattenDepth = 16;

// Always fixed notation with a fixed decimal count. Scientific notation ("1e-05")
// is rejected by several DXF readers including some laser controllers, and
// std::format is locale-independent so a comma-decimal locale cannot corrupt it.
[[nodiscard]] std::string dxfNumber(const double value)
{
    return std::format("{:.6f}", std::isfinite(value) ? value : 0.0);
}

void group(std::string& out, const int code, const std::string_view value)
{
    // Every DXF group is exactly two lines: the integer code, then the value.
    out += std::format("{}\r\n{}\r\n", code, value);
}

void group(std::string& out, const int code, const double value) { group(out, code, dxfNumber(value)); }

void groupInt(std::string& out, const int code, const int value) { group(out, code, std::to_string(value)); }

[[nodiscard]] int colourForRole(const LayerRole role)
{
    switch (role) {
    case LayerRole::Cut: return 1;       // red
    case LayerRole::Outline: return 5;   // blue
    case LayerRole::Smooth: return 8;    // dark grey
    case LayerRole::Hidden: return 9;    // light grey
    case LayerRole::Reference: return 6; // magenta
    }
    return 1;
}

[[nodiscard]] double degrees(const double radians)
{
    double value = std::fmod(radians * 180.0 / pi, 360.0);
    if (value < 0.0) value += 360.0;
    return value;
}

[[nodiscard]] bool isFlat(const Cubic& cubic, const double tolerance)
{
    const double dx = cubic.end.X() - cubic.start.X();
    const double dy = cubic.end.Y() - cubic.start.Y();
    const double chord = std::hypot(dx, dy);
    if (chord < tolerance) {
        // Degenerate chord: fall back to comparing control-point spread.
        return cubic.start.Distance(cubic.firstControl) < tolerance
            && cubic.start.Distance(cubic.secondControl) < tolerance;
    }
    const auto distance = [dx, dy, chord, &cubic](const gp_Pnt2d& control) {
        return std::abs(dy * (control.X() - cubic.start.X()) - dx * (control.Y() - cubic.start.Y())) / chord;
    };
    return distance(cubic.firstControl) <= tolerance && distance(cubic.secondControl) <= tolerance;
}

[[nodiscard]] gp_Pnt2d midpoint(const gp_Pnt2d& a, const gp_Pnt2d& b)
{
    return {0.5 * (a.X() + b.X()), 0.5 * (a.Y() + b.Y())};
}

// Flatten by recursive de Casteljau subdivision. Emits the end point of each
// flat-enough span, so the caller supplies the starting point.
void flattenCubic(const Cubic& cubic, const double tolerance, const int depth, std::vector<gp_Pnt2d>& out)
{
    if (depth >= maximumFlattenDepth || isFlat(cubic, tolerance)) {
        out.push_back(cubic.end);
        return;
    }
    const auto p01 = midpoint(cubic.start, cubic.firstControl);
    const auto p12 = midpoint(cubic.firstControl, cubic.secondControl);
    const auto p23 = midpoint(cubic.secondControl, cubic.end);
    const auto p012 = midpoint(p01, p12);
    const auto p123 = midpoint(p12, p23);
    const auto middle = midpoint(p012, p123);
    flattenCubic(Cubic{cubic.start, p01, p012, middle}, tolerance, depth + 1, out);
    flattenCubic(Cubic{middle, p123, p23, cubic.end}, tolerance, depth + 1, out);
}

struct Vertex final {
    gp_Pnt2d position;
    // tan(sweep/4) for the arc running from this vertex to the next; 0 for a
    // straight span. This is what lets one polyline carry exact circular arcs
    // interleaved with lines, which is precisely what CAM wants for a cut path.
    double bulge{};
};

void emitLine(std::string& out, const std::string& layer, const Segment& segment)
{
    group(out, 0, "LINE");
    group(out, 8, layer);
    group(out, 10, segment.start.X());
    group(out, 20, segment.start.Y());
    group(out, 30, 0.0);
    group(out, 11, segment.end.X());
    group(out, 21, segment.end.Y());
    group(out, 31, 0.0);
}

void emitCircle(std::string& out, const std::string& layer, const Arc& arc)
{
    group(out, 0, "CIRCLE");
    group(out, 8, layer);
    group(out, 10, arc.centre.X());
    group(out, 20, arc.centre.Y());
    group(out, 30, 0.0);
    group(out, 40, arc.radius);
}

void emitArc(std::string& out, const std::string& layer, const Arc& arc)
{
    // A DXF arc is always traced counter-clockwise from its start angle to its
    // end angle, so a clockwise sweep is expressed by swapping the two -- the
    // same point set, traced the other way.
    const double from = arc.sweepAngle >= 0.0 ? arc.startAngle : arc.startAngle + arc.sweepAngle;
    const double to = arc.sweepAngle >= 0.0 ? arc.startAngle + arc.sweepAngle : arc.startAngle;
    group(out, 0, "ARC");
    group(out, 8, layer);
    group(out, 10, arc.centre.X());
    group(out, 20, arc.centre.Y());
    group(out, 30, 0.0);
    group(out, 40, arc.radius);
    group(out, 50, degrees(from));
    group(out, 51, degrees(to));
}

void emitPolyline(std::string& out, const std::string& layer, const std::vector<Vertex>& vertices,
                  const bool closed)
{
    if (vertices.size() < 2) return;
    group(out, 0, "POLYLINE");
    group(out, 8, layer);
    groupInt(out, 66, 1); // vertices follow
    groupInt(out, 70, closed ? 1 : 0);
    group(out, 10, 0.0);
    group(out, 20, 0.0);
    group(out, 30, 0.0);
    for (const auto& vertex : vertices) {
        group(out, 0, "VERTEX");
        group(out, 8, layer);
        group(out, 10, vertex.position.X());
        group(out, 20, vertex.position.Y());
        group(out, 30, 0.0);
        if (vertex.bulge != 0.0) group(out, 42, vertex.bulge);
    }
    // Mandatory: omitting SEQEND corrupts everything after it in the file.
    group(out, 0, "SEQEND");
    group(out, 8, layer);
}

[[nodiscard]] std::vector<Vertex> verticesFor(const Contour& contour)
{
    std::vector<Vertex> vertices;
    for (const auto& primitive : contour.primitives) {
        if (const auto* segment = std::get_if<Segment>(&primitive)) {
            vertices.push_back({segment->start, 0.0});
        } else if (const auto* arc = std::get_if<Arc>(&primitive)) {
            vertices.push_back({arc->startPoint(), std::tan(arc->sweepAngle / 4.0)});
        } else {
            const auto& cubic = std::get<Cubic>(primitive);
            vertices.push_back({cubic.start, 0.0});
            std::vector<gp_Pnt2d> flattened;
            flattenCubic(cubic, cubicToleranceMm, 0, flattened);
            // The final flattened point is this primitive's end; the next
            // primitive contributes it, or the closing vertex below does.
            for (std::size_t index = 0; index + 1 < flattened.size(); ++index) {
                vertices.push_back({flattened[index], 0.0});
            }
        }
    }
    // An open polyline needs its final endpoint; a closed one gets the closing
    // span from the closed flag instead, so adding it would duplicate a vertex.
    if (!contour.closed && !contour.primitives.empty()) {
        vertices.push_back({primitiveEnd(contour.primitives.back()), 0.0});
    }
    return vertices;
}

void emitContour(std::string& out, const std::string& layer, const Contour& contour)
{
    if (contour.primitives.empty()) return;
    // A lone primitive gets the most faithful dedicated entity available.
    if (contour.primitives.size() == 1) {
        const auto& primitive = contour.primitives.front();
        if (const auto* segment = std::get_if<Segment>(&primitive)) {
            emitLine(out, layer, *segment);
            return;
        }
        if (const auto* arc = std::get_if<Arc>(&primitive)) {
            if (arc->isFullCircle()) emitCircle(out, layer, *arc);
            else emitArc(out, layer, *arc);
            return;
        }
    }
    emitPolyline(out, layer, verticesFor(contour), contour.closed);
}

void emitTables(std::string& out, const Drawing& drawing)
{
    group(out, 0, "SECTION");
    group(out, 2, "TABLES");

    // Every layer's linetype must resolve to an LTYPE entry, and CONTINUOUS is
    // the minimum a valid file needs. Dangling references are the main cause of
    // "drawing repaired" prompts.
    group(out, 0, "TABLE");
    group(out, 2, "LTYPE");
    groupInt(out, 70, 1);
    group(out, 0, "LTYPE");
    group(out, 2, "CONTINUOUS");
    groupInt(out, 70, 0);
    group(out, 3, "Solid line");
    groupInt(out, 72, 65);
    groupInt(out, 73, 0);
    group(out, 40, 0.0);
    group(out, 0, "ENDTAB");

    group(out, 0, "TABLE");
    group(out, 2, "LAYER");
    groupInt(out, 70, static_cast<int>(drawing.layers.size()));
    for (const auto& layer : drawing.layers) {
        // Layer 0 is reserved in DXF, and naming layers by role is what lets an
        // operator set power and speed per class of geometry.
        group(out, 0, "LAYER");
        group(out, 2, layer.name.empty() ? defaultLayerName(layer.role) : layer.name);
        groupInt(out, 70, 0);
        groupInt(out, 62, colourForRole(layer.role));
        group(out, 6, "CONTINUOUS");
    }
    group(out, 0, "ENDTAB");
    group(out, 0, "ENDSEC");
}

} // namespace

std::string DxfWriter::serialize(const Drawing& drawing, const DrawingWriteOptions& options,
                                 DrawingWriteResult& result) const
{
    const auto prepared = prepareForOutput(drawing, options);
    // No vertical mirror: DXF model space is Y-up, matching the drawing frame.
    const auto& output = prepared.drawing;

    std::string dxf;
    group(dxf, 0, "SECTION");
    group(dxf, 2, "HEADER");
    group(dxf, 9, "$ACADVER");
    group(dxf, 1, "AC1009");
    // Declare the page box, not the tight geometry bounds, so that this file
    // reports the same real-world size as the SVG and PDF written from the same
    // drawing -- their page includes the margin. Reporting tight bounds here made
    // DXF disagree with the other two by twice the margin, which the validator
    // correctly rejected. A reader's zoom-to-extents then shows the margin too,
    // which is what a viewer should show anyway.
    group(dxf, 9, "$EXTMIN");
    group(dxf, 10, 0.0);
    group(dxf, 20, 0.0);
    group(dxf, 30, 0.0);
    group(dxf, 9, "$EXTMAX");
    group(dxf, 10, prepared.pageWidthMm);
    group(dxf, 20, prepared.pageHeightMm);
    group(dxf, 30, 0.0);
    group(dxf, 0, "ENDSEC");

    emitTables(dxf, output);

    group(dxf, 0, "SECTION");
    group(dxf, 2, "ENTITIES");
    int contours = 0;
    for (const auto& layer : output.layers) {
        const auto name = layer.name.empty() ? defaultLayerName(layer.role) : layer.name;
        for (const auto& contour : layer.contours) {
            if (contour.primitives.empty()) continue;
            emitContour(dxf, name, contour);
            ++contours;
        }
    }
    group(dxf, 0, "ENDSEC");
    group(dxf, 0, "EOF");

    result.written = false;
    result.pageWidthMm = prepared.pageWidthMm;
    result.pageHeightMm = prepared.pageHeightMm;
    result.contoursWritten = contours;
    return dxf;
}

DrawingWriteResult DxfWriter::write(const Drawing& drawing, const std::filesystem::path& destination,
                                    const DrawingWriteOptions& options) const
{
    DrawingWriteResult result;
    const auto dxf = serialize(drawing, options, result);

    std::filesystem::create_directories(destination.parent_path());
    exporting::detail::AtomicExportFile partial(destination);
    {
        std::ofstream output(partial.partial(), std::ios::binary);
        if (!output) {
            throw DrawingWriteError(DrawingWriteError::Code::UnwritableDestination,
                                    "unable to open the DXF destination for writing");
        }
        output.write(dxf.data(), static_cast<std::streamsize>(dxf.size()));
        if (!output) {
            throw DrawingWriteError(DrawingWriteError::Code::EncodingFailed, "failed while writing the DXF");
        }
    }
    partial.finalize();
    result.written = true;
    return result;
}

} // namespace loupe::drawing
