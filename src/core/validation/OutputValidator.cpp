#include "core/validation/OutputValidator.h"

#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <Bnd_Box.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <NCollection_Sequence.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>

namespace loupe::validation {
namespace {

void error(ValidationResult& result, std::string code, std::string message)
{
    result.errors.push_back({std::move(code), std::move(message)});
}

OutputUnit unitForMeters(const double meters)
{
    if (std::abs(meters - 0.001) < 1.0e-12) return OutputUnit::Millimeter;
    if (std::abs(meters - 0.0254) < 1.0e-12) return OutputUnit::Inch;
    return OutputUnit::Unknown;
}

OutputUnit unitForName(const std::string_view name)
{
    std::string normalized(name);
    std::ranges::transform(normalized, normalized.begin(), [](const unsigned char value) { return static_cast<char>(std::toupper(value)); });
    if (normalized == "MM" || normalized == "MILLIMETRE" || normalized == "MILLIMETER") return OutputUnit::Millimeter;
    if (normalized == "IN" || normalized == "INCH") return OutputUnit::Inch;
    return OutputUnit::Unknown;
}

void setBounds(ValidationResult& result, const Bnd_Box& bounds, const double scale)
{
    if (bounds.IsVoid()) return;
    double minX, minY, minZ, maxX, maxY, maxZ;
    bounds.Get(minX, minY, minZ, maxX, maxY, maxZ);
    result.actualBoundsMm = {{minX * scale, minY * scale, minZ * scale}, {maxX * scale, maxY * scale, maxZ * scale}};
    result.actualCentroidMm = {(result.actualBoundsMm.minimum.x + result.actualBoundsMm.maximum.x) / 2.0,
                                (result.actualBoundsMm.minimum.y + result.actualBoundsMm.maximum.y) / 2.0,
                                (result.actualBoundsMm.minimum.z + result.actualBoundsMm.maximum.z) / 2.0};
}

bool close(const double actual, const double wanted, const double tolerance)
{
    return std::abs(actual - wanted) <= tolerance * std::max({1.0, std::abs(actual), std::abs(wanted)});
}

bool same(const Vec3& actual, const Vec3& wanted, const double tolerance)
{
    return close(actual.x, wanted.x, tolerance) && close(actual.y, wanted.y, tolerance) && close(actual.z, wanted.z, tolerance);
}

void compare(ValidationResult& result, const ExpectedOutput& expected)
{
    if (result.actualUnit != expected.unit) error(result, "unit_mismatch", "output unit differs from reviewed expectation");
    if (result.actualBodyCount != expected.bodyCount) error(result, "body_count_mismatch", "output body count differs from reviewed expectation");
    if (!same(result.actualBoundsMm.minimum, expected.boundsMm.minimum, expected.relativeTolerance)
        || !same(result.actualBoundsMm.maximum, expected.boundsMm.maximum, expected.relativeTolerance)) {
        error(result, "bounds_mismatch", "output bounds differ from reviewed expectation");
    }
    if (!same(result.actualCentroidMm, expected.centroidMm, expected.relativeTolerance)) {
        error(result, "centroid_mismatch", "output centroid differs from reviewed expectation");
    }
}

ValidationResult readStep(const ExpectedOutput& expected)
{
    ValidationResult result; result.path = expected.path;
    STEPCAFControl_Reader reader;
    if (reader.ReadFile(expected.path.string().c_str()) != IFSelect_RetDone) {
        error(result, "invalid_shape", "STEP file could not be read"); return result;
    }
    NCollection_Sequence<TCollection_AsciiString> lengthNames, angleNames, solidAngleNames;
    reader.ChangeReader().FileUnits(lengthNames, angleNames, solidAngleNames);
    occ::handle<TDocStd_Document> document;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", document);
    if (!reader.Transfer(document)) { error(result, "invalid_shape", "STEP file could not be transferred"); return result; }
    result.reopened = true;
    double meters = 0.0;
    const bool hasXcafUnit = XCAFDoc_DocumentTool::GetLengthUnit(document, meters);
    const OutputUnit xcafUnit = unitForMeters(meters);
    if (lengthNames.IsEmpty()) {
        error(result, "unit_ambiguous", "STEP declares no output length unit");
    } else {
        result.actualUnit = unitForName(lengthNames.Value(1).ToCString());
        for (int index = 2; index <= lengthNames.Length(); ++index) {
            if (unitForName(lengthNames.Value(index).ToCString()) != result.actualUnit) {
                error(result, "unit_ambiguous", "STEP declares conflicting output length units");
                break;
            }
        }
        if (result.actualUnit == OutputUnit::Unknown) error(result, "unit_ambiguous", "STEP output length unit is not supported");
    }
    if (hasXcafUnit && xcafUnit != OutputUnit::Unknown && result.actualUnit != OutputUnit::Unknown && xcafUnit != result.actualUnit) {
        result.warnings.push_back({"xcaf_unit_normalized", "XCAF geometry unit differs from the declared STEP output unit"});
    } else if (!hasXcafUnit || xcafUnit == OutputUnit::Unknown) {
        result.warnings.push_back({"xcaf_unit_missing", "XCAF did not retain the declared STEP output unit"});
    }
    const double toMillimeters = meters > 0.0 ? meters * 1000.0 : 1.0;
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    NCollection_Sequence<TDF_Label> freeShapes;
    shapes->GetFreeShapes(freeShapes);
    result.actualBodyCount = static_cast<std::size_t>(freeShapes.Length());
    Bnd_Box bounds;
    result.shapeValid = true;
    for (int index = 1; index <= freeShapes.Length(); ++index) {
        const TopoDS_Shape shape = shapes->GetShape(freeShapes.Value(index));
        if (shape.IsNull() || !BRepCheck_Analyzer(shape).IsValid()) result.shapeValid = false;
        BRepBndLib::Add(shape, bounds);
    }
    if (!result.shapeValid) error(result, "invalid_shape", "reopened STEP contains an invalid BRep shape");
    setBounds(result, bounds, toMillimeters);
    return result;
}

float floatAt(const std::array<char, 50>& bytes, const std::size_t offset)
{
    float value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

ValidationResult readStl(const ExpectedOutput& expected)
{
    ValidationResult result; result.path = expected.path; result.actualUnit = OutputUnit::Millimeter;
    std::ifstream input(expected.path, std::ios::binary);
    std::array<char, 80> header{}; std::uint32_t count{};
    input.read(header.data(), static_cast<std::streamsize>(header.size()));
    input.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!input) { error(result, "invalid_shape", "STL header or triangle count is invalid"); return result; }
    const auto size = std::filesystem::file_size(expected.path);
    constexpr std::uint64_t headerSize = 84;
    constexpr std::uint64_t triangleSize = 50;
    if (count > (std::numeric_limits<std::uint64_t>::max() - headerSize) / triangleSize
        || size != headerSize + triangleSize * static_cast<std::uint64_t>(count)) {
        error(result, "invalid_shape", "STL byte payload does not match its triangle count"); return result;
    }
    result.reopened = true; result.shapeValid = count > 0; result.actualBodyCount = count > 0 ? 1 : 0;
    Bnd_Box bounds;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::array<char, 50> triangle{};
        input.read(triangle.data(), static_cast<std::streamsize>(triangle.size()));
        if (!input) { result.shapeValid = false; error(result, "invalid_shape", "STL triangle data is truncated"); break; }
        const Vec3 normal{floatAt(triangle, 0), floatAt(triangle, 4), floatAt(triangle, 8)};
        std::array<Vec3, 3> vertices{};
        for (std::size_t vertex = 0; vertex < vertices.size(); ++vertex) {
            vertices[vertex] = {floatAt(triangle, 12 + vertex * 12), floatAt(triangle, 16 + vertex * 12), floatAt(triangle, 20 + vertex * 12)};
            if (!std::isfinite(vertices[vertex].x) || !std::isfinite(vertices[vertex].y) || !std::isfinite(vertices[vertex].z)) {
                result.shapeValid = false; error(result, "invalid_shape", "STL contains non-finite vertex coordinates");
            }
            bounds.Update(vertices[vertex].x, vertices[vertex].y, vertices[vertex].z);
        }
        const Vec3 ab{vertices[1].x - vertices[0].x, vertices[1].y - vertices[0].y, vertices[1].z - vertices[0].z};
        const Vec3 ac{vertices[2].x - vertices[0].x, vertices[2].y - vertices[0].y, vertices[2].z - vertices[0].z};
        const Vec3 cross{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
        const double normalLengthSquared = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
        const double areaSquared = cross.x * cross.x + cross.y * cross.y + cross.z * cross.z;
        if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z)) {
            result.shapeValid = false; error(result, "invalid_shape", "STL contains non-finite normals");
        } else if (normalLengthSquared == 0.0 || areaSquared == 0.0) {
            result.shapeValid = false; error(result, "degenerate_triangle", "STL contains a zero-normal or degenerate triangle");
        } else if (cross.x * normal.x + cross.y * normal.y + cross.z * normal.z < 0.0) {
            result.shapeValid = false; error(result, "mirrored_triangle_orientation", "STL triangle winding does not agree with its normal");
        }
    }
    if (count == 0) error(result, "invalid_shape", "STL contains no triangles");
    setBounds(result, bounds, 1.0);
    return result;
}

} // namespace

ValidationResult OutputValidator::validate(const ExpectedOutput& expected) const
{
    ValidationResult result; result.path = expected.path;
    if (!std::isfinite(expected.relativeTolerance) || expected.relativeTolerance <= 0.0) {
        error(result, "invalid_tolerance", "relative tolerance must be finite and positive"); return result;
    }
    if (!std::filesystem::exists(expected.path)) { error(result, "missing_file", "output file does not exist"); return result; }
    const auto extension = expected.path.extension().string();
    result = extension == ".stl" || extension == ".STL" ? readStl(expected) : readStep(expected);
    if (result.reopened) compare(result, expected);
    result.passed = result.reopened && result.shapeValid && result.errors.empty();
    return result;
}

std::vector<ValidationResult> OutputValidator::validateBatch(const std::vector<ExpectedOutput>& expected) const
{
    std::vector<ValidationResult> results;
    results.reserve(expected.size());
    for (const auto& row : expected) results.push_back(validate(row));
    return results;
}

} // namespace loupe::validation
