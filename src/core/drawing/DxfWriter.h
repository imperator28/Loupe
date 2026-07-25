#pragma once

#include "core/drawing/DrawingWriter.h"

#include <filesystem>
#include <string>

namespace loupe::drawing {

// DXF R12 (AC1009) writer -- the format shop software actually consumes.
//
// R12 deliberately, because it has the widest compatibility with older CAM and
// inexpensive laser controllers and far less required structure to get right.
// Its one real weakness is that R12 predates $INSUNITS, so the file is formally
// unitless and "one drawing unit is one millimetre" is a convention; the optional
// scale fiducial exists to make that convention checkable on the shop floor.
//
// Note this writer does NOT mirror vertically. DXF model space is Y-up, unlike
// SVG and PDF.
class DxfWriter final {
public:
    [[nodiscard]] DrawingWriteResult write(const Drawing& drawing, const std::filesystem::path& destination,
                                           const DrawingWriteOptions& options = {}) const;

    [[nodiscard]] std::string serialize(const Drawing& drawing, const DrawingWriteOptions& options,
                                        DrawingWriteResult& result) const;
};

} // namespace loupe::drawing
