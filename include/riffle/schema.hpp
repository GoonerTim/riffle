#pragma once

#include <expected>
#include <span>
#include <string>

#include "riffle/types.hpp"

namespace riffle {

// Reduce a column's observed types to a single type per the conflict policy.
std::expected<ColumnType, std::string> resolve_type_conflict(std::span<const ColumnType> seen,
                                                             TypeConflictPolicy policy);

}  // namespace riffle
