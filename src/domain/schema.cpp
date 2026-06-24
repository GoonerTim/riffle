#include "riffle/schema.hpp"

#include <algorithm>
#include <ranges>
#include <string>
#include <variant>
#include <vector>

#include "riffle/factories.hpp"
#include "riffle/timestamp.hpp"

namespace riffle {
namespace {

ColumnType infer_type(const CellValue& value) {
    auto type = column_type_of(value);
    if (type == ColumnType::STRING && looks_like_timestamp(std::get<std::string>(value))) {
        return ColumnType::TIMESTAMP;
    }
    return type;
}

}  // namespace

std::expected<void, std::string> InferenceSink::field(std::string_view path, CellValue value) {
    auto [it, inserted] = seen_.try_emplace(std::string(path));
    if (inserted) order_.push_back(it->first);
    auto type = infer_type(value);
    if (std::ranges::find(it->second, type) == it->second.end()) it->second.push_back(type);
    return {};
}

std::expected<void, std::string> InferenceSink::end_row() {
    ++rows_;
    return {};
}

InferredSchema InferenceSink::schema() const {
    InferredSchema schema;
    schema.sampled_rows = rows_;
    for (const auto& path : order_) {
        auto resolved = resolve_type_conflict(seen_.at(path), policy_);
        auto type = resolved ? *resolved : ColumnType::STRING;
        schema.columns.push_back(make_ColumnSchema({.name = path, .type = type}));
    }
    return make_InferredSchema(schema);
}

namespace {

ColumnSchema* find_by_name(std::vector<ColumnSchema>& columns, const std::string& name) {
    for (auto& column : columns) {
        if (column.name == name) return &column;
    }
    return nullptr;
}

void apply_override(std::vector<ColumnSchema>& columns, const ColumnSchema& column) {
    if (auto* existing = find_by_name(columns, column.name)) {
        existing->type = column.type;
        existing->nullable = column.nullable;
        return;
    }
    columns.push_back(column);
}

}  // namespace

InferredSchema merge_override(InferredSchema inferred, const InferredSchema& override) {
    for (const auto& column : override.columns) {
        apply_override(inferred.columns, column);
    }
    return make_InferredSchema(inferred);
}

}  // namespace riffle
