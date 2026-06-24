#include "riffle/schema.hpp"

#include <algorithm>
#include <map>
#include <ranges>
#include <string>
#include <vector>

#include "riffle/factories.hpp"

namespace riffle {
namespace {

// Accumulates, per flattened path, the distinct observed types in arrival order.
struct Accum {
    std::vector<std::string> order;
    std::map<std::string, std::vector<ColumnType>> seen;
};

void observe(Accum& acc, const Field& field) {
    auto& types = acc.seen[field.path];
    if (types.empty()) acc.order.push_back(field.path);
    auto type = column_type_of(field.value);
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

}  // namespace riffle
