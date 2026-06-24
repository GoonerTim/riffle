#pragma once

#include <expected>
#include <span>
#include <string>

#include "riffle/ports.hpp"
#include "riffle/types.hpp"

namespace riffle {

// Reduce a column's observed types to a single type per the conflict policy.
std::expected<ColumnType, std::string> resolve_type_conflict(std::span<const ColumnType> seen,
                                                             TypeConflictPolicy policy);

// Infer an output schema from up to INFER_SAMPLE_ROWS rows of the source.
InferredSchema infer_schema(RowSource& source, TypeConflictPolicy policy);

// Overlay an explicit schema onto an inferred one (replace by name, else append).
InferredSchema merge_override(InferredSchema inferred, const InferredSchema& override);

}  // namespace riffle
