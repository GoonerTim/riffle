#include "riffle/schema.hpp"

#include <algorithm>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "riffle/factories.hpp"
#include "riffle/timestamp.hpp"

namespace riffle {
namespace {

ColumnType infer_type(const CellValue& value) {
    auto type = column_type_of(value);
    if (type == ColumnType::STRING && looks_like_timestamp(std::get<std::string_view>(value))) {
        return ColumnType::TIMESTAMP;
    }
    return type;
}

}

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

}

InferredSchema merge_override(InferredSchema inferred, const InferredSchema& override) {
    for (const auto& column : override.columns) {
        apply_override(inferred.columns, column);
    }
    return make_InferredSchema(inferred);
}

namespace {

const ColumnSchema* find_const(const std::vector<ColumnSchema>& columns, const std::string& name) {
    for (const auto& col : columns) {
        if (col.name == name) return &col;
    }
    return nullptr;
}

std::expected<std::vector<ColumnSchema>, std::string> select_columns(
    const std::vector<ColumnSchema>& columns, const std::vector<std::string>& select) {
    std::vector<ColumnSchema> out;
    for (const auto& name : select) {
        const auto* col = find_const(columns, name);
        if (!col) return std::unexpected("unknown column in --select: " + name);
        out.push_back(*col);
    }
    return out;
}

bool contains(const std::vector<std::string>& names, const std::string& name) {
    return std::ranges::find(names, name) != names.end();
}

void rename_columns(std::vector<ColumnSchema>& columns,
                    const std::vector<std::pair<std::string, std::string>>& renames) {
    for (const auto& [from, to] : renames) {
        if (auto* col = find_by_name(columns, from)) col->name = to;
    }
}

bool has_duplicates(const std::vector<ColumnSchema>& columns) {
    std::set<std::string> seen;
    for (const auto& col : columns) {
        if (!seen.insert(col.name).second) return true;
    }
    return false;
}

}  // namespace

std::expected<InferredSchema, std::string> apply_projection(InferredSchema schema,
                                                            const Projection& projection) {
    auto columns = schema.columns;
    if (!projection.select.empty()) {
        auto selected = select_columns(columns, projection.select);
        if (!selected) return std::unexpected(selected.error());
        columns = std::move(*selected);
    }
    std::erase_if(columns,
                  [&](const ColumnSchema& c) { return contains(projection.exclude, c.name); });
    rename_columns(columns, projection.rename);
    if (has_duplicates(columns))
        return std::unexpected("projection produced duplicate column names");
    schema.columns = std::move(columns);
    return schema;
}

}
