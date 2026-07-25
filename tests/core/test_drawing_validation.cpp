#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/drawing/DrawingValidator.h"
#include "core/drawing/DxfWriter.h"
#include "core/drawing/PdfWriter.h"
#include "core/drawing/SvgWriter.h"

#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <string_view>

namespace {

using loupe::drawing::Arc;
using loupe::drawing::Contour;
using loupe::drawing::Drawing;
using loupe::drawing::DrawingWriteOptions;
using loupe::drawing::DxfWriter;
using loupe::drawing::ExpectedDrawing;
using loupe::drawing::Layer;
using loupe::drawing::LayerRole;
using loupe::drawing::PdfWriter;
using loupe::drawing::Primitive;
using loupe::drawing::Segment;
using loupe::drawing::SvgWriter;

constexpr double pi = std::numbers::pi;

class ScopedDirectory {
public:
    explicit ScopedDirectory(const std::string_view name)
        : path_(std::filesystem::temp_directory_path() / ("loupe-drawing-" + std::string(name)))
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~ScopedDirectory() { std::filesystem::remove_all(path_); }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

DrawingWriteOptions noMargin()
{
    DrawingWriteOptions options;
    options.marginMm = 0.0;
    return options;
}

Drawing drawingWith(std::vector<Primitive> primitives, const bool closed = true)
{
    Drawing drawing;
    Layer layer;
    layer.role = LayerRole::Cut;
    layer.name = loupe::drawing::defaultLayerName(LayerRole::Cut);
    layer.contours.push_back(Contour{std::move(primitives), closed});
    drawing.layers.push_back(std::move(layer));
    return drawing;
}

// Chiral L-bracket with a notch: 30 wide, 60 tall. A mirrored copy is obvious.
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

TEST_CASE("every format round-trips a chiral part at its true size", "[drawing-validation]")
{
    // The single most important test in the feature: one part, three formats, and
    // each file independently re-read and confirmed to declare 30 x 60 mm. A scale
    // error in any writer, or a page computed differently by any one of them,
    // fails here.
    const ScopedDirectory directory{"roundtrip"};
    const auto drawing = chiralBracket();

    static_cast<void>(SvgWriter{}.write(drawing, directory.path() / "bracket.svg", noMargin()));
    static_cast<void>(DxfWriter{}.write(drawing, directory.path() / "bracket.dxf", noMargin()));
    static_cast<void>(PdfWriter{}.write(drawing, directory.path() / "bracket.pdf", noMargin()));

    for (const auto* name : {"bracket.svg", "bracket.dxf", "bracket.pdf"}) {
        const auto result = loupe::drawing::validateDrawing({directory.path() / name, 30.0, 60.0, 0.01});
        CAPTURE(name);
        REQUIRE(result.reopened);
        REQUIRE(result.errors.empty());
        REQUIRE(result.passed);
        REQUIRE(result.declaredWidthMm == Catch::Approx(30.0).margin(0.01));
        REQUIRE(result.declaredHeightMm == Catch::Approx(60.0).margin(0.01));
    }
}

TEST_CASE("a drawing containing arcs round-trips at true size", "[drawing-validation]")
{
    // Arcs take a different path in every writer -- native in SVG and DXF,
    // Bezier-approximated in PDF -- so size has to be confirmed with them too.
    const ScopedDirectory directory{"arcs"};
    const auto drawing = drawingWith({Arc{{10.0, 10.0}, 10.0, 0.0, 2.0 * pi}}, true);

    static_cast<void>(SvgWriter{}.write(drawing, directory.path() / "disc.svg", noMargin()));
    static_cast<void>(DxfWriter{}.write(drawing, directory.path() / "disc.dxf", noMargin()));
    static_cast<void>(PdfWriter{}.write(drawing, directory.path() / "disc.pdf", noMargin()));

    for (const auto* name : {"disc.svg", "disc.dxf", "disc.pdf"}) {
        const auto result = loupe::drawing::validateDrawing({directory.path() / name, 20.0, 20.0, 0.01});
        CAPTURE(name);
        REQUIRE(result.passed);
    }
}

TEST_CASE("all three formats agree on page size with a non-zero margin",
          "[drawing-validation]")
{
    // Regression: every writer test originally used a zero margin, so the margin
    // path went unexercised and DXF declared tight geometry bounds while SVG and
    // PDF declared a page including the margin -- a disagreement of twice the
    // margin, caught only when the spike ran with default options on real
    // geometry.
    const ScopedDirectory directory{"margin"};
    DrawingWriteOptions options;
    options.marginMm = 2.0;

    const auto drawing = chiralBracket();
    const auto svg = SvgWriter{}.write(drawing, directory.path() / "m.svg", options);
    const auto dxf = DxfWriter{}.write(drawing, directory.path() / "m.dxf", options);
    const auto pdf = PdfWriter{}.write(drawing, directory.path() / "m.pdf", options);

    // 30 x 60 part plus 2 mm on every side.
    REQUIRE(svg.pageWidthMm == Catch::Approx(34.0));
    REQUIRE(svg.pageHeightMm == Catch::Approx(64.0));
    REQUIRE(dxf.pageWidthMm == Catch::Approx(svg.pageWidthMm));
    REQUIRE(pdf.pageWidthMm == Catch::Approx(svg.pageWidthMm));

    for (const auto* name : {"m.svg", "m.dxf", "m.pdf"}) {
        const auto result = loupe::drawing::validateDrawing({directory.path() / name, 34.0, 64.0, 0.01});
        CAPTURE(name);
        REQUIRE(result.passed);
        REQUIRE(result.declaredWidthMm == Catch::Approx(34.0).margin(0.01));
        REQUIRE(result.declaredHeightMm == Catch::Approx(64.0).margin(0.01));
    }
}

TEST_CASE("validation rejects a file that declares the wrong size", "[drawing-validation]")
{
    const ScopedDirectory directory{"wrong-size"};
    static_cast<void>(SvgWriter{}.write(chiralBracket(), directory.path() / "bracket.svg", noMargin()));

    // Ask for a size the file does not have; this is what a scale bug looks like.
    const auto result = loupe::drawing::validateDrawing({directory.path() / "bracket.svg", 60.0, 120.0, 0.01});

    REQUIRE(result.reopened);
    REQUIRE_FALSE(result.passed);
    REQUIRE(result.errors.size() == 1);
    REQUIRE(result.errors.front().code == "size_mismatch");
}

TEST_CASE("validation rejects an SVG whose viewBox contradicts its millimetre size",
          "[drawing-validation]")
{
    // This is the specific way an SVG silently stops being 1:1: the physical size
    // says one thing and the user-unit box says another.
    const ScopedDirectory directory{"bad-viewbox"};
    const auto path = directory.path() / "bad.svg";
    {
        std::ofstream output(path, std::ios::binary);
        output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"30mm\" height=\"60mm\" "
                  "viewBox=\"0 0 113 227\"></svg>";
    }

    const auto result = loupe::drawing::validateDrawing({path, 30.0, 60.0, 0.01});

    REQUIRE(result.reopened);
    REQUIRE_FALSE(result.passed);
    REQUIRE(result.errors.front().code == "viewbox_scale_mismatch");
}

TEST_CASE("validation rejects a truncated file", "[drawing-validation]")
{
    const ScopedDirectory directory{"truncated"};
    const auto path = directory.path() / "cut.dxf";
    {
        std::ofstream output(path, std::ios::binary);
        output << "0\r\nSECTION\r\n2\r\nHEADER\r\n";
    }

    const auto result = loupe::drawing::validateDrawing({path, 0.0, 0.0, 0.01});

    REQUIRE(result.reopened);
    REQUIRE_FALSE(result.passed);
}

TEST_CASE("validation rejects a missing file and an unknown format", "[drawing-validation]")
{
    const ScopedDirectory directory{"missing"};

    const auto missing = loupe::drawing::validateDrawing({directory.path() / "absent.dxf", 10.0, 10.0, 0.01});
    REQUIRE_FALSE(missing.reopened);
    REQUIRE(missing.errors.front().code == "missing_file");

    const auto unknown = loupe::drawing::validateDrawing({directory.path() / "thing.step", 10.0, 10.0, 0.01});
    REQUIRE(unknown.errors.front().code == "unsupported_format");
}

TEST_CASE("validation rejects a non-positive tolerance", "[drawing-validation]")
{
    const auto result = loupe::drawing::validateDrawing({"anything.dxf", 10.0, 10.0, 0.0});

    REQUIRE_FALSE(result.passed);
    REQUIRE(result.errors.front().code == "invalid_tolerance");
}

TEST_CASE("written files are atomic and leave no partial behind", "[drawing-validation]")
{
    const ScopedDirectory directory{"atomic"};
    const auto path = directory.path() / "bracket.dxf";
    const auto result = DxfWriter{}.write(chiralBracket(), path, noMargin());

    REQUIRE(result.written);
    REQUIRE(std::filesystem::exists(path));
    // A reader must never observe a half-written drawing.
    int strays = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory.path())) {
        if (entry.path().filename().string().find(".partial") != std::string::npos) ++strays;
    }
    REQUIRE(strays == 0);
}

TEST_CASE("the fiducial is measurable in the written file", "[drawing-validation]")
{
    // The fiducial only earns its place if it really is the stated length, since
    // its whole purpose is to let an operator confirm scale by measuring.
    const ScopedDirectory directory{"fiducial"};
    DrawingWriteOptions options;
    options.marginMm = 0.0;
    options.includeScaleFiducial = true;
    options.fiducialLengthMm = 50.0;

    const auto path = directory.path() / "with-fiducial.dxf";
    const auto written = DxfWriter{}.write(chiralBracket(), path, options);

    const auto result = loupe::drawing::validateDrawing(
        {path, written.pageWidthMm, written.pageHeightMm, 0.01});
    REQUIRE(result.passed);
    // The bracket is 30 wide, so a 50 mm fiducial widens the page to 50.
    REQUIRE(result.declaredWidthMm == Catch::Approx(50.0).margin(0.01));
}

} // namespace
