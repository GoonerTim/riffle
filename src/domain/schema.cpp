#include "riffle/schema.hpp"

#include <algorithm>
#include <map>
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

// Accumulates, per flattened path, the distinct observed types in arrival order.
struct Accum {
    std::vector<std::string> order;
    std::map<std::string, std::vector<ColumnType>> seen;
};

void observe(Accum& acc, const Field& field) {
    auto& types = acc.seen[field.path];
    if (types.empty()) acc.order.push_back(field.path);
    auto type = infer_type(field.value);
    if (std::ranges::find(types, type) == types.end()) types.push_back(type);
}

void observe_row(Accum& acc, const Row& row) {
    for (const auto& field : row.fields) observe(acc, field);
}

ColumnSchema resolve_column(const Accum& acc, const std::string& path, TypeConflictPolicy policy) {
    auto resolved = resolve_type_conflict(acc.seen.at(path), policy);
    auto type = resolved ? *resolved : ColumnType::STRING;
    return make_ColumnSchema({.name = path, .type = type});
}

InferredSchema build_schema(const Accum& acc, TypeConflictPolicy policy, std::size_t rows) {
    InferredSchema schema;
    schema.sampled_rows = rows;
    for (const auto& path : acc.order) {
        schema.columns.push_back(resolve_column(acc, path, policy));
    }
    return make_InferredSchema(schema);
}

}  // namespace

InferredSchema infer_schema(RowSource& source, TypeConflictPolicy policy) {
    Accum acc;
    std::size_t rows = 0;
    for (auto row = source.next(); row && rows < INFER_SAMPLE_ROWS; row = source.next()) {
        observe_row(acc, *row);
        ++rows;
    }
    return build_schema(acc, policy, rows);
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
