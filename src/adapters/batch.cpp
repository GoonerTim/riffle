#include "riffle/batch.hpp"

#include <arrow/api.h>

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include "riffle/schema.hpp"
#include "riffle/timestamp.hpp"

namespace riffle {
namespace {

std::expected<void, std::string> check(const arrow::Status& status) {
    if (status.ok()) return {};
    return std::unexpected(status.ToString());
}

std::shared_ptr<arrow::DataType> arrow_type(ColumnType type) {
    switch (type) {
        case ColumnType::INT64: return arrow::int64();
        case ColumnType::DOUBLE: return arrow::float64();
        case ColumnType::BOOL: return arrow::boolean();
        case ColumnType::STRING: return arrow::utf8();
        case ColumnType::TIMESTAMP: return arrow::timestamp(arrow::TimeUnit::MICRO, "UTC");
        default: return arrow::null();
    }
}

std::shared_ptr<arrow::ArrayBuilder> make_array_builder(ColumnType type) {
    switch (type) {
        case ColumnType::INT64: return std::make_shared<arrow::Int64Builder>();
        case ColumnType::DOUBLE: return std::make_shared<arrow::DoubleBuilder>();
        case ColumnType::BOOL: return std::make_shared<arrow::BooleanBuilder>();
        case ColumnType::STRING: return std::make_shared<arrow::StringBuilder>();
        case ColumnType::TIMESTAMP:
            return std::make_shared<arrow::TimestampBuilder>(
                arrow::timestamp(arrow::TimeUnit::MICRO, "UTC"), arrow::default_memory_pool());
        default: return std::make_shared<arrow::NullBuilder>();
    }
}

CellValue find_value(const Row& row, const std::string& path) {
    for (const auto& field : row.fields) {
        if (field.path == path) return field.value;
    }
    return CellValue{};
}

std::string to_text(const CellValue& value) {
    if (auto* v = std::get_if<std::int64_t>(&value)) return std::to_string(*v);
    if (auto* v = std::get_if<double>(&value)) return std::to_string(*v);
    if (auto* v = std::get_if<bool>(&value)) return *v ? "true" : "false";
    if (auto* v = std::get_if<std::string>(&value)) return *v;
    return {};
}

template <class B>
B& as(ColumnBuilder& column) {
    return *static_cast<B*>(column.builder.get());
}

template <class A>
auto as_array(const arrow::Array& array, std::int64_t i) {
    return static_cast<const A&>(array).Value(i);
}

std::expected<void, std::string> append_null(ColumnBuilder& column) {
    ++column.null_count;
    return check(column.builder->AppendNull());
}

std::expected<void, std::string> append_int(ColumnBuilder& column, const CellValue& value) {
    if (auto* v = std::get_if<std::int64_t>(&value)) return check(as<arrow::Int64Builder>(column).Append(*v));
    return std::unexpected("int64 column got non-integer value");
}

std::expected<void, std::string> append_double(ColumnBuilder& column, const CellValue& value) {
    if (auto* v = std::get_if<std::int64_t>(&value)) return check(as<arrow::DoubleBuilder>(column).Append(static_cast<double>(*v)));
    if (auto* v = std::get_if<double>(&value)) return check(as<arrow::DoubleBuilder>(column).Append(*v));
    return std::unexpected("double column got non-numeric value");
}

std::expected<void, std::string> append_bool(ColumnBuilder& column, const CellValue& value) {
    if (auto* v = std::get_if<bool>(&value)) return check(as<arrow::BooleanBuilder>(column).Append(*v));
    return std::unexpected("bool column got non-boolean value");
}

std::expected<void, std::string> append_string(ColumnBuilder& column, const CellValue& value) {
    return check(as<arrow::StringBuilder>(column).Append(to_text(value)));
}

std::expected<void, std::string> append_timestamp(ColumnBuilder& column, const CellValue& value) {
    auto* text = std::get_if<std::string>(&value);
    if (!text) return std::unexpected("timestamp column got non-string value");
    auto micros = parse_timestamp_us(*text);
    if (!micros) return std::unexpected("invalid timestamp: " + *text);
    return check(as<arrow::TimestampBuilder>(column).Append(*micros));
}

std::expected<void, std::string> append_cell(ColumnBuilder& column, const CellValue& value) {
    if (std::holds_alternative<std::monostate>(value)) return append_null(column);
    switch (column.schema.type) {
        case ColumnType::INT64: return append_int(column, value);
        case ColumnType::DOUBLE: return append_double(column, value);
        case ColumnType::BOOL: return append_bool(column, value);
        case ColumnType::STRING: return append_string(column, value);
        case ColumnType::TIMESTAMP: return append_timestamp(column, value);
        default: return append_null(column);
    }
}

std::expected<void, std::string> finish_column(ColumnBuilder& column,
                                               std::vector<std::shared_ptr<arrow::Array>>& arrays,
                                               arrow::FieldVector& fields) {
    std::shared_ptr<arrow::Array> array;
    if (auto ok = check(column.builder->Finish(&array)); !ok) return ok;
    arrays.push_back(array);
    fields.push_back(arrow::field(column.schema.name, arrow_type(column.schema.type)));
    column.null_count = 0;
    return {};
}

std::string text_at(const arrow::Array& array, std::int64_t i, ColumnType from) {
    switch (from) {
        case ColumnType::INT64: return std::to_string(as_array<arrow::Int64Array>(array, i));
        case ColumnType::DOUBLE: return std::to_string(as_array<arrow::DoubleArray>(array, i));
        case ColumnType::BOOL: return as_array<arrow::BooleanArray>(array, i) ? "true" : "false";
        case ColumnType::TIMESTAMP: return std::to_string(as_array<arrow::TimestampArray>(array, i));
        case ColumnType::STRING: return std::string(static_cast<const arrow::StringArray&>(array).GetView(i));
        default: return {};
    }
}

std::expected<void, std::string> refill_value(arrow::ArrayBuilder& dst, const arrow::Array& src,
                                              std::int64_t i, ColumnType from, ColumnType to) {
    if (to == ColumnType::DOUBLE) {
        return check(static_cast<arrow::DoubleBuilder&>(dst).Append(
            static_cast<double>(as_array<arrow::Int64Array>(src, i))));
    }
    if (to == ColumnType::STRING) {
        return check(static_cast<arrow::StringBuilder&>(dst).Append(text_at(src, i, from)));
    }
    return std::unexpected("unsupported widening target");
}

std::expected<void, std::string> refill(arrow::ArrayBuilder& dst, const arrow::Array& src,
                                        ColumnType from, ColumnType to) {
    for (std::int64_t i = 0; i < src.length(); ++i) {
        if (src.IsNull(i)) {
            if (auto ok = check(dst.AppendNull()); !ok) return ok;
            continue;
        }
        if (auto ok = refill_value(dst, src, i, from, to); !ok) return ok;
    }
    return {};
}

bool cell_fits(ColumnType column, const CellValue& value) {
    if (std::holds_alternative<std::monostate>(value)) return true;
    auto type = column_type_of(value);
    if (type == column) return true;
    if (column == ColumnType::DOUBLE && type == ColumnType::INT64) return true;
    if (column == ColumnType::TIMESTAMP && type == ColumnType::STRING) return true;
    return column == ColumnType::STRING;
}

}  // namespace

std::shared_ptr<arrow::Schema> arrow_schema_of(const InferredSchema& schema) {
    arrow::FieldVector fields;
    for (const auto& column : schema.columns) {
        fields.push_back(arrow::field(column.name, arrow_type(column.type)));
    }
    return arrow::schema(fields);
}

BatchBuilder make_batch_builder(const InferredSchema& schema) {
    BatchBuilder builder;
    for (const auto& column : schema.columns) {
        builder.columns.push_back({column, make_array_builder(column.type), 0});
    }
    return builder;
}

namespace {

std::expected<void, std::string> ensure_fits(ColumnBuilder& column, const CellValue& value,
                                             TypeConflictPolicy policy) {
    if (cell_fits(column.schema.type, value)) return {};
    const std::array<ColumnType, 2> seen{column.schema.type, column_type_of(value)};
    auto target = resolve_type_conflict(seen, policy);
    if (!target) return std::unexpected(target.error());
    return widen_column(column, *target);
}

}  // namespace

std::expected<void, std::string> append_row(BatchBuilder& builder, const Row& row,
                                            TypeConflictPolicy policy) {
    for (auto& column : builder.columns) {
        auto value = find_value(row, column.schema.json_path);
        if (auto ok = ensure_fits(column, value, policy); !ok) return ok;
        if (auto ok = append_cell(column, value); !ok) return ok;
    }
    ++builder.n_rows;
    return {};
}

std::expected<RecordBatch, std::string> build_batch(BatchBuilder& builder) {
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    arrow::FieldVector fields;
    for (auto& column : builder.columns) {
        if (auto ok = finish_column(column, arrays, fields); !ok) return std::unexpected(ok.error());
    }
    auto data = arrow::RecordBatch::Make(arrow::schema(fields), builder.n_rows, arrays);
    const std::size_t rows = std::exchange(builder.n_rows, 0);
    return RecordBatch{data, rows};
}

std::expected<void, std::string> widen_column(ColumnBuilder& column, ColumnType to) {
    const ColumnType from = column.schema.type;
    std::shared_ptr<arrow::Array> old;
    if (auto ok = check(column.builder->Finish(&old)); !ok) return ok;
    column.builder = make_array_builder(to);
    column.schema.type = to;
    return refill(*column.builder, *old, from, to);
}

}  // namespace riffle
