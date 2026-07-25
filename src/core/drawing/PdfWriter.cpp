#include "core/drawing/PdfWriter.h"
#include "core/export/AtomicExportFile.h"

#include <array>
#include <cmath>
#include <format>
#include <fstream>
#include <numbers>
#include <vector>

namespace loupe::drawing {
namespace {

constexpr double pi = std::numbers::pi;
// PDF's unit is exactly 1/72 inch, by specification. This is the entire reason
// PDF carries no scale ambiguity.
constexpr double pointsPerMillimetre = 72.0 / 25.4;
// Cut paths are hairlines; 0.1 mm reads clearly on screen and in print without
// obscuring the geometry.
constexpr double strokeWidthMm = 0.1;

[[nodiscard]] double toPoints(const double millimetres) { return millimetres * pointsPerMillimetre; }

[[nodiscard]] std::string number(const double value) { return formatNumber(value, 4); }

[[nodiscard]] std::array<double, 3> colourForRole(const LayerRole role)
{
    switch (role) {
    case LayerRole::Cut: return {0.0, 0.0, 0.0};
    case LayerRole::Outline: return {0.0, 0.0, 0.8};
    case LayerRole::Smooth: return {0.5, 0.5, 0.5};
    case LayerRole::Hidden: return {0.7, 0.7, 0.7};
    case LayerRole::Reference: return {0.8, 0.0, 0.8};
    }
    return {0.0, 0.0, 0.0};
}

void moveTo(std::string& out, const gp_Pnt2d& point)
{
    out += std::format("{} {} m\n", number(toPoints(point.X())), number(toPoints(point.Y())));
}

void lineTo(std::string& out, const gp_Pnt2d& point)
{
    out += std::format("{} {} l\n", number(toPoints(point.X())), number(toPoints(point.Y())));
}

void curveTo(std::string& out, const gp_Pnt2d& first, const gp_Pnt2d& second, const gp_Pnt2d& end)
{
    out += std::format("{} {} {} {} {} {} c\n", number(toPoints(first.X())), number(toPoints(first.Y())),
                       number(toPoints(second.X())), number(toPoints(second.Y())),
                       number(toPoints(end.X())), number(toPoints(end.Y())));
}

// PDF has no arc primitive, so arcs become cubic Beziers. With segments of at
// most 90 degrees the maximum radial error is about 0.027 percent of the radius
// -- for a 10 mm hole that is under 3 micrometres, far below any kerf.
void appendArc(std::string& out, const Arc& arc)
{
    const int segments = std::max(1, static_cast<int>(std::ceil(std::abs(arc.sweepAngle) / (pi / 2.0))));
    const double step = arc.sweepAngle / segments;
    const double control = (4.0 / 3.0) * std::tan(step / 4.0);

    double angle = arc.startAngle;
    for (int index = 0; index < segments; ++index) {
        const double next = angle + step;
        const gp_Pnt2d start = arc.pointAtAngle(angle);
        const gp_Pnt2d end = arc.pointAtAngle(next);
        const gp_Pnt2d first(start.X() - arc.radius * control * std::sin(angle),
                             start.Y() + arc.radius * control * std::cos(angle));
        const gp_Pnt2d second(end.X() + arc.radius * control * std::sin(next),
                              end.Y() - arc.radius * control * std::cos(next));
        curveTo(out, first, second, end);
        angle = next;
    }
}

void appendContour(std::string& out, const Contour& contour)
{
    if (contour.primitives.empty()) return;
    moveTo(out, primitiveStart(contour.primitives.front()));
    for (const auto& primitive : contour.primitives) {
        if (const auto* segment = std::get_if<Segment>(&primitive)) {
            lineTo(out, segment->end);
        } else if (const auto* arc = std::get_if<Arc>(&primitive)) {
            appendArc(out, *arc);
        } else {
            const auto& cubic = std::get<Cubic>(primitive);
            curveTo(out, cubic.firstControl, cubic.secondControl, cubic.end);
        }
    }
    if (contour.closed) out += "h\n";
    out += "S\n";
}

} // namespace

std::string PdfWriter::serialize(const Drawing& drawing, const DrawingWriteOptions& options,
                                 DrawingWriteResult& result) const
{
    const auto prepared = prepareForOutput(drawing, options);
    // No mirror: PDF user space is y-up, like the drawing frame.
    const auto& output = prepared.drawing;

    std::string content;
    content += std::format("{} w\n1 J\n1 j\n", number(toPoints(strokeWidthMm)));
    int contours = 0;
    for (const auto& layer : output.layers) {
        const auto colour = colourForRole(layer.role);
        content += std::format("{} {} {} RG\n", number(colour[0]), number(colour[1]), number(colour[2]));
        for (const auto& contour : layer.contours) {
            if (contour.primitives.empty()) continue;
            appendContour(content, contour);
            ++contours;
        }
    }

    // Uncompressed and font-free: this is line art, so compression would only
    // make the output harder to inspect when something goes wrong.
    std::vector<std::string> objects;
    objects.push_back("<< /Type /Catalog /Pages 2 0 R >>");
    objects.push_back("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    objects.push_back(std::format("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {} {}] "
                                  "/Contents 4 0 R /Resources << >> >>",
                                  number(toPoints(prepared.pageWidthMm)),
                                  number(toPoints(prepared.pageHeightMm))));
    objects.push_back(std::format("<< /Length {} >>\nstream\n{}endstream", content.size(), content));

    std::string pdf = "%PDF-1.4\n";
    std::vector<std::size_t> offsets;
    offsets.reserve(objects.size());
    for (std::size_t index = 0; index < objects.size(); ++index) {
        offsets.push_back(pdf.size());
        pdf += std::format("{} 0 obj\n{}\nendobj\n", index + 1, objects[index]);
    }

    const std::size_t xrefOffset = pdf.size();
    pdf += std::format("xref\n0 {}\n", objects.size() + 1);
    // Every xref entry is exactly twenty bytes wide, including the trailing
    // space before the newline; readers depend on that fixed width.
    pdf += "0000000000 65535 f \n";
    for (const auto offset : offsets) pdf += std::format("{:010d} 00000 n \n", offset);
    pdf += std::format("trailer\n<< /Size {} /Root 1 0 R >>\nstartxref\n{}\n%%EOF\n",
                       objects.size() + 1, xrefOffset);

    result.written = false;
    result.pageWidthMm = prepared.pageWidthMm;
    result.pageHeightMm = prepared.pageHeightMm;
    result.contoursWritten = contours;
    return pdf;
}

DrawingWriteResult PdfWriter::write(const Drawing& drawing, const std::filesystem::path& destination,
                                    const DrawingWriteOptions& options) const
{
    DrawingWriteResult result;
    const auto pdf = serialize(drawing, options, result);

    std::filesystem::create_directories(destination.parent_path());
    exporting::detail::AtomicExportFile partial(destination);
    {
        std::ofstream output(partial.partial(), std::ios::binary);
        if (!output) {
            throw DrawingWriteError(DrawingWriteError::Code::UnwritableDestination,
                                    "unable to open the PDF destination for writing");
        }
        output.write(pdf.data(), static_cast<std::streamsize>(pdf.size()));
        if (!output) {
            throw DrawingWriteError(DrawingWriteError::Code::EncodingFailed, "failed while writing the PDF");
        }
    }
    partial.finalize();
    result.written = true;
    return result;
}

} // namespace loupe::drawing
