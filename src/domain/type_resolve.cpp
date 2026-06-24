#include "riffle/schema.hpp"

#include <algorithm>
#include <vector>

namespace riffle {
namespace {

std::vector<ColumnType> distinct_non_null(std::span<const ColumnType> seen) {
    std::vector<ColumnType> out;
    for (auto type : seen) {
        if (type == ColumnType::NULLTYPE) continue;
        if (std::ranges::find(out, type) == out.end()) out.push_back(type);
    }
    return out;
}

bool all_numeric(const std::vector<ColumnType>& types) {
    return std::ranges::all_of(types, [](ColumnType t) {
        return t == ColumnType::INT64 || t == ColumnType::DOUBLE;
    });
}

ColumnType widen(const std::vector<ColumnType>& types) {
    return all_numeric(types) ? ColumnType::DOUBLE : ColumnType::STRING;
}

}  // namespace

std::expected<ColumnType, std::string> resolve_type_conflict(std::span<const ColumnType> seen,
                                                             TypeConflictPolicy policy) {
    const auto types = distinct_non_null(seen);
    if (types.empty()) return ColumnType::NULLTYPE;
    if (types.size() == 1) return types.front();
    if (policy == TypeConflictPolicy::STRING) return ColumnType::STRING;
    if (policy == TypeConflictPolicy::WIDEN) return widen(types);
    return std::unexpected("conflicting column types");
}

}  // namespace riffle
