#pragma once

#include "core/domain/AssemblyTypes.h"

#include <Standard_Handle.hxx>
#include <TDocStd_Document.hxx>
#include <TDF_Label.hxx>

namespace loupe::import::detail {

// Test-only seam for exercising XCAF hierarchy traversal without STEP writer flattening.
[[nodiscard]] domain::AssemblySnapshot snapshotForTesting(const occ::handle<TDocStd_Document>& document);
[[nodiscard]] domain::AssemblySnapshot snapshotForTesting(const occ::handle<TDocStd_Document>& document, const TDF_Label& root);

} // namespace loupe::import::detail
