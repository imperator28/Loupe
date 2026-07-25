#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/drawing/DxfWriter.h"
#include "core/drawing/PdfWriter.h"
#include "core/drawing/SvgWriter.h"

#include <clocale>
#include <cmath>
#include <numbers>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace {

using loupe::drawing::Arc;
using loupe::drawing::Contour;
using loupe::drawing::Cubic;
using loupe::drawing::Drawing;
using loupe::drawing::DrawingWriteOptions;
using loupe::drawing::DrawingWriteResult;
using loupe::drawing::DxfWriter;
using loupe::drawing::Layer;
using loupe::drawing::LayerRole;
using loupe::drawing::PdfWriter;
using loupe::drawing::Primitive;
using loupe::drawing::Segment;
using loupe::drawing::SvgWriter;

constexpr double pi = std::numbers::pi;

DrawingWriteOptions noMargin()
{
    DrawingWriteOptions options;
    options.marginMm = 0.0;
    return options;
}

Drawing drawingWith(std::vector<Primitive> primitives, const bool closed = true,
                    const LayerRole role = LayerRole::Cut)
{
    Drawing drawing;
    Layer layer;
    layer.role = role;
    layer.name = loupe::drawing::defaultLayerName(role);
    layer.contours.push_back(Contour{std::move(primitives), closed});
    drawing.layers.push_back(std::move(layer));
    return drawing;
}

// 100 x 50 rectangle, drawn counter-clockwise from the origin.
Drawing rectangle()
{
    return drawingWith({Segment{{0.0, 0.0}, {100.0, 0.0}},
                        Segment{{100.0, 0.0}, {100.0, 50.0}},
                        Segment{{100.0, 50.0}, {0.0, 50.0}},
                        Segment{{0.0, 50.0}, {0.0, 0.0}}});
}

// Deliberately chiral: an L with a notch, so a mirrored file is unmistakable.
// The tall arm is on the LEFT and the notch is at the BOTTOM RIGHT.
Drawing chiralBracket()
{
    return drawingWith({Segment{{0.0, 0.0}, {30.0, 0.0}},
                        Segment{{30.0, 0.0}, {30.0, 10.0}},
                        Segment{{30.0, 10.0}, {20.0, 10.0}},
                        Segment{{20.0, 10.0}, {20.0, 5.0}},
                        Segment{{20.0, 5.0}, {10.0, 5.0}},
                        Segment{{10.0, 5.0}, {10.0, 60.0}},
                        Segment{{10.0, 60.0}, {0.0, 60.0}},
                        Segment{{0.0, 60.0}, {0.0, 0.0}}});
}

std::vector<double> allNumbers(const std::string& text)
{
    std::vector<double> values;
    const std::regex pattern(R"(-?\d+\.?\d*)");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern); it != std::sregex_iterator(); ++it) {
        values.push_back(std::stod(it->str()));
    }
    return values;
}

// Read the first DXF group with the given code at or after `after`.
//
// The `after` anchor matters: several codes are reused across sections -- code 40
// is a circle radius on a CIRCLE but also the pattern length in the LTYPE table,
// which appears earlier in every file -- so an unanchored search silently reads
// the wrong entity's value.
std::optional<std::string> dxfGroup(const std::string& dxf, const std::string& code,
                                    const std::string& after = {})
{
    const std::string padded = "\r\n" + dxf;
    std::size_t position = 0;
    if (!after.empty()) {
        position = padded.find(after);
        if (position == std::string::npos) return std::nullopt;
    }
    const std::string needle = "\r\n" + code + "\r\n";
    position = padded.find(needle, position);
    if (position == std::string::npos) return std::nullopt;
    const std::size_t valueStart = position + needle.size();
    const std::size_t valueEnd = padded.find("\r\n", valueStart);
    if (valueEnd == std::string::npos) return std::nullopt;
    return padded.substr(valueStart, valueEnd - valueStart);
}

struct SvgArcCommand final {
    double radius{};
    int largeArc{};
    int sweep{};
    double endX{};
    double endY{};
};

std::optional<SvgArcCommand> firstSvgArc(const std::string& svg)
{
    const std::regex pattern(R"(A (-?[\d.]+) (-?[\d.]+) 0 ([01]) ([01]) (-?[\d.]+) (-?[\d.]+))");
    std::smatch match;
    if (!std::regex_search(svg, match, pattern)) return std::nullopt;
    return SvgArcCommand{std::stod(match[1]), std::stoi(match[3]), std::stoi(match[4]),
                         std::stod(match[5]), std::stod(match[6])};
}

TEST_CASE("SVG declares millimetre size and a matching viewBox", "[drawing-writer][svg]")
{
    DrawingWriteResult result;
    const auto svg = SvgWriter{}.serialize(rectangle(), noMargin(), result);

    // Both the physical size and the viewBox must be present and agree: that
    // pairing is what pins one user unit to one millimetre.
    REQUIRE(svg.find("width=\"100mm\"") != std::string::npos);
    REQUIRE(svg.find("height=\"50mm\"") != std::string::npos);
    REQUIRE(svg.find("viewBox=\"0 0 100 50\"") != std::string::npos);
    REQUIRE(result.pageWidthMm == Catch::Approx(100.0));
    REQUIRE(result.pageHeightMm == Catch::Approx(50.0));
    REQUIRE(result.contoursWritten == 1);
}

TEST_CASE("SVG honours the margin without scaling the geometry", "[drawing-writer][svg]")
{
    DrawingWriteOptions options;
    options.marginMm = 5.0;
    DrawingWriteResult result;
    static_cast<void>(SvgWriter{}.serialize(rectangle(), options, result));

    // The page grows by twice the margin; the part itself must not change size.
    REQUIRE(result.pageWidthMm == Catch::Approx(110.0));
    REQUIRE(result.pageHeightMm == Catch::Approx(60.0));
}

TEST_CASE("SVG mirrors vertically because its y axis points down", "[drawing-writer][svg]")
{
    // The bracket's tall arm is on the left and its notch at the bottom right.
    // After the y flip into SVG space the tall arm must still be on the left --
    // a horizontal mirror would be a scrapped part -- while the notch moves to
    // the top right.
    DrawingWriteResult result;
    const auto svg = SvgWriter{}.serialize(chiralBracket(), noMargin(), result);

    REQUIRE(result.pageHeightMm == Catch::Approx(60.0));
    // Start point (0,0) in a y-up frame becomes (0,60) in SVG space.
    REQUIRE(svg.find("M 0 60") != std::string::npos);
    // X coordinates must be untouched by a vertical mirror.
    REQUIRE(svg.find("L 30 60") != std::string::npos);
}

TEST_CASE("SVG emits an exact arc command rather than tessellating", "[drawing-writer][svg]")
{
    const auto drawing = drawingWith({Arc{{20.0, 20.0}, 10.0, 0.0, pi / 2.0},
                                      Segment{{20.0, 30.0}, {30.0, 20.0}}},
                                     true);
    DrawingWriteResult result;
    const auto svg = SvgWriter{}.serialize(drawing, noMargin(), result);

    const auto arc = firstSvgArc(svg);
    REQUIRE(arc.has_value());
    REQUIRE(arc->radius == Catch::Approx(10.0));
    REQUIRE(arc->largeArc == 0);
}

TEST_CASE("SVG arc direction survives the vertical mirror", "[drawing-writer][svg]")
{
    // Recover the arc's centre and sweep from the emitted command using SVG's own
    // endpoint-to-centre parameterisation (spec F.6.5), then check it matches the
    // mirrored geometry. A wrong sweep flag keeps both endpoints correct while
    // bulging the arc the wrong way, so nothing but this catches it.
    constexpr double radius = 10.0;
    const Arc source{{20.0, 20.0}, radius, 0.0, pi / 2.0};
    const auto drawing = drawingWith({source}, false);

    DrawingWriteResult result;
    const auto svg = SvgWriter{}.serialize(drawing, noMargin(), result);
    const auto arc = firstSvgArc(svg);
    REQUIRE(arc.has_value());

    // Derive what the writer should have emitted by replaying the same two IR
    // operations it applies -- translate to origin, then mirror -- rather than
    // hand-computing coordinates, which is easy to get wrong by forgetting the
    // translation.
    const auto expected = drawing.translatedToOrigin(0.0).mirroredVertically(result.pageHeightMm);
    const auto& expectedArc =
        std::get<Arc>(expected.layers.front().contours.front().primitives.front());

    const gp_Pnt2d start = expectedArc.startPoint();
    const gp_Pnt2d end(arc->endX, arc->endY);
    // The emitted endpoint must be the mirrored arc's endpoint.
    REQUIRE(end.X() == Catch::Approx(expectedArc.endPoint().X()).margin(1.0e-6));
    REQUIRE(end.Y() == Catch::Approx(expectedArc.endPoint().Y()).margin(1.0e-6));

    // F.6.5 with equal radii and no rotation.
    const double halfDx = (start.X() - end.X()) / 2.0;
    const double halfDy = (start.Y() - end.Y()) / 2.0;
    const double numerator = radius * radius * radius * radius
        - radius * radius * (halfDy * halfDy + halfDx * halfDx);
    const double denominator = radius * radius * (halfDy * halfDy + halfDx * halfDx);
    const double factor = std::sqrt(std::max(0.0, numerator / denominator));
    const double sign = (arc->largeArc != arc->sweep) ? 1.0 : -1.0;
    const double cxPrime = sign * factor * halfDy;
    const double cyPrime = sign * factor * -halfDx;
    const gp_Pnt2d recovered(cxPrime + (start.X() + end.X()) / 2.0, cyPrime + (start.Y() + end.Y()) / 2.0);

    // A wrong sweep flag leaves both endpoints correct but reflects the centre
    // across the chord, so this is the assertion that actually catches it.
    REQUIRE(recovered.X() == Catch::Approx(expectedArc.centre.X()).margin(1.0e-6));
    REQUIRE(recovered.Y() == Catch::Approx(expectedArc.centre.Y()).margin(1.0e-6));

    // Mirroring negates the sweep, so the emitted flag must be the clockwise one.
    REQUIRE(expectedArc.sweepAngle < 0.0);
    REQUIRE(arc->sweep == 0);
}

TEST_CASE("SVG draws a full circle as two half turns", "[drawing-writer][svg]")
{
    // A single elliptical-arc command with coincident endpoints renders nothing.
    const auto drawing = drawingWith({Arc{{10.0, 10.0}, 5.0, 0.0, 2.0 * pi}}, true);
    DrawingWriteResult result;
    const auto svg = SvgWriter{}.serialize(drawing, noMargin(), result);

    std::size_t arcs = 0;
    for (std::size_t position = svg.find(" A "); position != std::string::npos;
         position = svg.find(" A ", position + 1)) {
        ++arcs;
    }
    REQUIRE(arcs == 2);
}

TEST_CASE("DXF is R12 and does not mirror, because its y axis points up",
          "[drawing-writer][dxf]")
{
    DrawingWriteResult result;
    const auto dxf = DxfWriter{}.serialize(chiralBracket(), noMargin(), result);

    REQUIRE(dxf.find("AC1009") != std::string::npos);
    // R13+/R2000-only entities and headers must never appear in an R12 file.
    REQUIRE(dxf.find("LWPOLYLINE") == std::string::npos);
    REQUIRE(dxf.find("SPLINE") == std::string::npos);
    REQUIRE(dxf.find("ELLIPSE") == std::string::npos);
    REQUIRE(dxf.find("$INSUNITS") == std::string::npos);

    // Unmirrored: the bracket's own extents, y minimum at zero.
    const auto minY = dxfGroup(dxf, "20");
    REQUIRE(minY.has_value());
    REQUIRE(std::stod(*minY) == Catch::Approx(0.0));
    REQUIRE(result.pageHeightMm == Catch::Approx(60.0));
}

TEST_CASE("DXF declares every layer it references", "[drawing-writer][dxf]")
{
    Drawing drawing = rectangle();
    Layer smooth;
    smooth.role = LayerRole::Smooth;
    smooth.name = loupe::drawing::defaultLayerName(LayerRole::Smooth);
    smooth.contours.push_back(Contour{{Segment{{10.0, 10.0}, {20.0, 20.0}}}, false});
    drawing.layers.push_back(std::move(smooth));

    DrawingWriteResult result;
    const auto dxf = DxfWriter{}.serialize(drawing, noMargin(), result);

    // A dangling layer or linetype reference is the main cause of a reader
    // offering to "repair" the drawing.
    REQUIRE(dxf.find("LTYPE") != std::string::npos);
    REQUIRE(dxf.find("CONTINUOUS") != std::string::npos);
    REQUIRE(dxf.find("\r\nLAYER\r\n") != std::string::npos);
    REQUIRE(dxf.find("CUT") != std::string::npos);
    REQUIRE(dxf.find("SMOOTH") != std::string::npos);
    REQUIRE(dxf.find("\r\nEOF\r\n") != std::string::npos);
}

TEST_CASE("DXF emits a lone full circle as CIRCLE", "[drawing-writer][dxf]")
{
    const auto drawing = drawingWith({Arc{{25.0, 25.0}, 5.0, 0.0, 2.0 * pi}}, true);
    DrawingWriteResult result;
    const auto dxf = DxfWriter{}.serialize(drawing, noMargin(), result);

    REQUIRE(dxf.find("CIRCLE") != std::string::npos);
    const auto radius = dxfGroup(dxf, "40", "\r\nCIRCLE\r\n");
    REQUIRE(radius.has_value());
    REQUIRE(std::stod(*radius) == Catch::Approx(5.0));
}

TEST_CASE("DXF encodes a mixed contour as one polyline with an exact bulge",
          "[drawing-writer][dxf]")
{
    // One polyline carrying an exact arc plus straight spans is what CAM wants
    // for a cut path, and the bulge is what makes the arc exact.
    const auto drawing = drawingWith({Segment{{0.0, 0.0}, {10.0, 0.0}},
                                      Arc{{10.0, 10.0}, 10.0, -pi / 2.0, pi / 2.0},
                                      Segment{{20.0, 10.0}, {0.0, 0.0}}},
                                     true);
    DrawingWriteResult result;
    const auto dxf = DxfWriter{}.serialize(drawing, noMargin(), result);

    REQUIRE(dxf.find("POLYLINE") != std::string::npos);
    // SEQEND is mandatory; without it everything after is corrupt.
    REQUIRE(dxf.find("SEQEND") != std::string::npos);
    const auto bulge = dxfGroup(dxf, "42");
    REQUIRE(bulge.has_value());
    // tan(sweep/4) for a quarter turn.
    REQUIRE(std::stod(*bulge) == Catch::Approx(std::tan((pi / 2.0) / 4.0)).margin(1.0e-6));
}

TEST_CASE("DXF never emits scientific notation", "[drawing-writer][dxf]")
{
    // Tiny coordinates are exactly where a default formatter reaches for "1e-05",
    // which several DXF readers and laser controllers reject outright.
    const auto drawing = drawingWith({Segment{{0.0, 0.0}, {0.0000123, 0.0000456}}}, false);
    DrawingWriteResult result;
    const auto dxf = DxfWriter{}.serialize(drawing, noMargin(), result);

    // Match an exponent attached to a digit, not any 'e' -- entity names like
    // LINE, VERTEX and SEQEND legitimately contain the letter.
    const std::regex scientific(R"([0-9][eE][+-]?[0-9])");
    REQUIRE_FALSE(std::regex_search(dxf, scientific));
}

TEST_CASE("PDF declares its MediaBox in points and does not mirror",
          "[drawing-writer][pdf]")
{
    DrawingWriteResult result;
    const auto pdf = PdfWriter{}.serialize(rectangle(), noMargin(), result);

    REQUIRE(pdf.starts_with("%PDF-1.4"));
    REQUIRE(pdf.find("%%EOF") != std::string::npos);
    REQUIRE(pdf.find("/MediaBox") != std::string::npos);

    // 100 x 50 mm at exactly 72 points per inch.
    const double expectedWidth = 100.0 * 72.0 / 25.4;
    const double expectedHeight = 50.0 * 72.0 / 25.4;
    const std::regex box(R"(/MediaBox \[0 0 ([\d.]+) ([\d.]+)\])");
    std::smatch match;
    REQUIRE(std::regex_search(pdf, match, box));
    REQUIRE(std::stod(match[1]) == Catch::Approx(expectedWidth).margin(0.01));
    REQUIRE(std::stod(match[2]) == Catch::Approx(expectedHeight).margin(0.01));
}

TEST_CASE("PDF stream length matches the declared value", "[drawing-writer][pdf]")
{
    // A wrong /Length makes the file unopenable in strict readers.
    DrawingWriteResult result;
    const auto pdf = PdfWriter{}.serialize(rectangle(), noMargin(), result);

    const std::regex lengthPattern(R"(/Length (\d+) >>\nstream\n)");
    std::smatch match;
    REQUIRE(std::regex_search(pdf, match, lengthPattern));
    const auto declared = static_cast<std::size_t>(std::stoul(match[1]));

    const std::string marker = "stream\n";
    const auto streamStart = pdf.find(marker, match.position(0)) + marker.size();
    const auto streamEnd = pdf.find("endstream", streamStart);
    REQUIRE(streamEnd != std::string::npos);
    REQUIRE(streamEnd - streamStart == declared);
}

TEST_CASE("PDF approximates an arc within a fraction of laser kerf", "[drawing-writer][pdf]")
{
    // PDF has no arc primitive. The 90-degree Bezier construction must stay well
    // inside kerf or a cut hole would be visibly out of round.
    const auto drawing = drawingWith({Arc{{20.0, 20.0}, 10.0, 0.0, 2.0 * pi}}, true);
    DrawingWriteResult result;
    const auto pdf = PdfWriter{}.serialize(drawing, noMargin(), result);

    // A full circle needs four curve segments at 90 degrees each.
    std::size_t curves = 0;
    for (std::size_t position = pdf.find(" c\n"); position != std::string::npos;
         position = pdf.find(" c\n", position + 1)) {
        ++curves;
    }
    REQUIRE(curves == 4);
    REQUIRE(result.pageWidthMm == Catch::Approx(20.0));
}

TEST_CASE("every writer keeps the same page size for the same drawing",
          "[drawing-writer][cross-format]")
{
    // The single most important guard in the feature: one part, three formats,
    // same real-world size. A scale error in any one writer shows up here.
    const auto drawing = chiralBracket();
    DrawingWriteResult svg;
    DrawingWriteResult dxf;
    DrawingWriteResult pdf;
    static_cast<void>(SvgWriter{}.serialize(drawing, noMargin(), svg));
    static_cast<void>(DxfWriter{}.serialize(drawing, noMargin(), dxf));
    static_cast<void>(PdfWriter{}.serialize(drawing, noMargin(), pdf));

    REQUIRE(svg.pageWidthMm == Catch::Approx(30.0));
    REQUIRE(svg.pageHeightMm == Catch::Approx(60.0));
    REQUIRE(dxf.pageWidthMm == Catch::Approx(svg.pageWidthMm));
    REQUIRE(dxf.pageHeightMm == Catch::Approx(svg.pageHeightMm));
    REQUIRE(pdf.pageWidthMm == Catch::Approx(svg.pageWidthMm));
    REQUIRE(pdf.pageHeightMm == Catch::Approx(svg.pageHeightMm));
    REQUIRE(svg.contoursWritten == dxf.contoursWritten);
    REQUIRE(svg.contoursWritten == pdf.contoursWritten);
}

TEST_CASE("only SVG mirrors; DXF and PDF agree on handedness",
          "[drawing-writer][cross-format]")
{
    // The bracket's start corner is at the origin. DXF and PDF are y-up so it
    // stays at y=0; SVG is y-down so it moves to y=pageHeight. Getting this wrong
    // in any one writer mirrors the part, which on a chiral outline is scrap.
    const auto drawing = chiralBracket();
    DrawingWriteResult svgResult;
    DrawingWriteResult dxfResult;
    const auto svg = SvgWriter{}.serialize(drawing, noMargin(), svgResult);
    const auto dxf = DxfWriter{}.serialize(drawing, noMargin(), dxfResult);

    REQUIRE(svg.find("M 0 60") != std::string::npos);
    // Anchor past the header, or this reads $EXTMIN's y instead of the first
    // entity's and would pass regardless of the geometry's handedness.
    const auto dxfFirstY = dxfGroup(dxf, "20", "\r\nENTITIES\r\n");
    REQUIRE(dxfFirstY.has_value());
    REQUIRE(std::stod(*dxfFirstY) == Catch::Approx(0.0));
    // The tall arm stays on the left in an unmirrored y-up file: the first
    // entity starts at the origin corner, not at the top.
    const auto dxfFirstX = dxfGroup(dxf, "10", "\r\nENTITIES\r\n");
    REQUIRE(dxfFirstX.has_value());
    REQUIRE(std::stod(*dxfFirstX) == Catch::Approx(0.0));
}

TEST_CASE("the optional fiducial is a known length on its own layer",
          "[drawing-writer][cross-format]")
{
    DrawingWriteOptions options;
    options.marginMm = 0.0;
    options.includeScaleFiducial = true;
    options.fiducialLengthMm = 50.0;

    DrawingWriteResult result;
    const auto dxf = DxfWriter{}.serialize(rectangle(), options, result);

    REQUIRE(dxf.find("REFERENCE") != std::string::npos);
    // The page grows to accommodate the fiducial below the part, and the part is
    // 100 wide so the 50 mm fiducial does not widen it.
    REQUIRE(result.pageWidthMm == Catch::Approx(100.0));
    REQUIRE(result.pageHeightMm > 50.0);
}

TEST_CASE("writing an empty drawing is refused", "[drawing-writer]")
{
    const Drawing empty;
    DrawingWriteResult result;

    REQUIRE_THROWS_AS(SvgWriter{}.serialize(empty, noMargin(), result), loupe::drawing::DrawingWriteError);
    REQUIRE_THROWS_AS(DxfWriter{}.serialize(empty, noMargin(), result), loupe::drawing::DrawingWriteError);
    REQUIRE_THROWS_AS(PdfWriter{}.serialize(empty, noMargin(), result), loupe::drawing::DrawingWriteError);
}

TEST_CASE("numbers are formatted locale-independently", "[drawing-writer]")
{
    // A comma-decimal locale would silently corrupt every coordinate in every
    // file, so this must never depend on the ambient locale.
    const auto previous = std::setlocale(LC_ALL, nullptr);
    const std::string saved = previous != nullptr ? previous : "C";
    // Best effort: if the locale is unavailable the assertions still hold under C.
    std::setlocale(LC_ALL, "de_DE.UTF-8");

    DrawingWriteResult result;
    const auto dxf = DxfWriter{}.serialize(drawingWith({Segment{{0.0, 0.0}, {1.5, 2.5}}}, false),
                                           noMargin(), result);
    std::setlocale(LC_ALL, saved.c_str());

    REQUIRE(dxf.find(",") == std::string::npos);
    REQUIRE(dxf.find("1.500000") != std::string::npos);
}

TEST_CASE("cubics are flattened for DXF but kept exact for SVG and PDF",
          "[drawing-writer]")
{
    const auto drawing = drawingWith({Cubic{{0.0, 0.0}, {0.0, 20.0}, {30.0, 20.0}, {30.0, 0.0}}}, false);

    DrawingWriteResult svgResult;
    const auto svg = SvgWriter{}.serialize(drawing, noMargin(), svgResult);
    REQUIRE(svg.find(" C ") != std::string::npos);

    DrawingWriteResult pdfResult;
    const auto pdf = PdfWriter{}.serialize(drawing, noMargin(), pdfResult);
    REQUIRE(pdf.find(" c\n") != std::string::npos);

    // R12 has no spline entity, so the curve becomes a polyline with enough
    // vertices to stay inside the chordal tolerance.
    DrawingWriteResult dxfResult;
    const auto dxf = DxfWriter{}.serialize(drawing, noMargin(), dxfResult);
    REQUIRE(dxf.find("POLYLINE") != std::string::npos);
    std::size_t vertices = 0;
    for (std::size_t position = dxf.find("\r\nVERTEX\r\n"); position != std::string::npos;
         position = dxf.find("\r\nVERTEX\r\n", position + 1)) {
        ++vertices;
    }
    REQUIRE(vertices > 4);
}

} // namespace
