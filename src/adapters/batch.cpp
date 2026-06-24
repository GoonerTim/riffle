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

std::string to_text(const CellValue& value) {
    if (auto* v = std::get_if<std::int64_t>(&value)) return std::to_string(*v);
    if (auto* v = std::get_if<double>(&value)) return std::to_string(*v);
    if (auto* v = std::get_if<bool>(&value)) return *v ? "true" : "false";
    if (auto* v = std::get_if<std::string>(&value)) return *v;
    return {};
}

void push_null(ColumnBuilder& column) {
    switch (column.schema.type) {
        case ColumnType::DOUBLE: column.doubles.push_back(0); break;
        case ColumnType::STRING: column.strings.emplace_back(); break;
        default: column.ints.push_back(0); break;
    }
    column.valid.push_back(0);
    ++column.null_count;
}

std::expected<void, std::string> push_int(ColumnBuilder& column, const CellValue& value) {
    auto* v = std::get_if<std::int64_t>(&value);
    if (!v) return std::unexpected("int64 column got non-integer value");
    column.ints.push_back(*v);
    return {};
}

std::expected<void, std::string> push_double(ColumnBuilder& column, const CellValue& value) {
    if (auto* v = std::get_if<std::int64_t>(&value)) column.doubles.push_back(static_cast<double>(*v));
    else if (auto* v = std::get_if<double>(&value)) column.doubles.push_back(*v);
    else return std::unexpected("double column got non-numeric value");
    return {};
}

std::expected<void, std::string> push_bool(ColumnBuilder& column, const CellValue& value) {
    auto* v = std::get_if<bool>(&value);
    if (!v) return std::unexpected("bool column got non-boolean value");
    column.ints.push_back(*v ? 1 : 0);
    return {};
}

std::expected<void, std::string> push_timestamp(ColumnBuilder& column, const CellValue& value) {
    auto* text = std::get_if<std::string>(&value);
    if (!text) return std::unexpected("timestamp column got non-string value");
    auto micros = parse_timestamp_us(*text);
    if (!micros) return std::unexpected("invalid timestamp: " + *text);
    column.ints.push_back(*micros);
    return {};
}

std::expected<void, std::string> push_typed(ColumnBuilder& column, const CellValue& value) {
    switch (column.schema.type) {
        case ColumnType::INT64: return push_int(column, value);
        case ColumnType::DOUBLE: return push_double(column, value);
        case ColumnType::BOOL: return push_bool(column, value);
        case ColumnType::TIMESTAMP: return push_timestamp(column, value);
        case ColumnType::STRING: column.strings.push_back(to_text(value)); return {};
        default: return {};
    }
}

std::expected<void, std::string> append_cell(ColumnBuilder& column, const CellValue& value) {
    if (std::holds_alternative<std::monostate>(value)) {
        push_null(column);
        return {};
    }
    if (auto ok = push_typed(column, value); !ok) return ok;
    column.valid.push_back(1);
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

const std::uint8_t* valid_ptr(const ColumnBuilder& column) {
    return column.null_count == 0 ? nullptr : column.valid.data();
}

std::expected<std::shared_ptr<arrow::Array>, std::string> finish(arrow::ArrayBuilder& builder) {
    std::shared_ptr<arrow::Array> array;
    if (auto ok = check(builder.Finish(&array)); !ok) return std::unexpected(ok.error());
    return array;
}

template <class Builder, class T>
std::expected<std::shared_ptr<arrow::Array>, std::string> finish_nums(Builder& builder,
                                                                      const std::vector<T>& values,
                                                                      const std::uint8_t* valid) {
    if (auto ok = check(builder.AppendValues(values.data(), values.size(), valid)); !ok) {
        return std::unexpected(ok.error());
    }
    return finish(builder);
}

std::expected<std::shared_ptr<arrow::Array>, std::string> finish_numeric(ColumnBuilder& column) {
    const auto* valid = valid_ptr(column);
    if (column.schema.type == ColumnType::DOUBLE) {
        arrow::DoubleBuilder builder;
        return finish_nums(builder, column.doubles, valid);
    }
    if (column.schema.type == ColumnType::TIMESTAMP) {
        arrow::TimestampBuilder builder(arrow_type(column.schema.type), arrow::default_memory_pool());
        return finish_nums(builder, column.ints, valid);
    }
    arrow::Int64Builder builder;
    return finish_nums(builder, column.ints, valid);
}

std::expected<std::shared_ptr<arrow::Array>, std::string> finish_bool(ColumnBuilder& column) {
    std::vector<std::uint8_t> bytes(column.ints.begin(), column.ints.end());
    arrow::BooleanBuilder builder;
    return finish_nums(builder, bytes, valid_ptr(column));
}

std::expected<std::shared_ptr<arrow::Array>, std::string> finish_string(ColumnBuilder& column) {
    arrow::StringBuilder builder;
    if (auto ok = check(builder.AppendValues(column.strings, valid_ptr(column))); !ok) return std::unexpected(ok.error());
    return finish(builder);
}

std::expected<std::shared_ptr<arrow::Array>, std::string> finish_array(ColumnBuilder& column) {
    if (column.schema.type == ColumnType::BOOL) return finish_bool(column);
    if (column.schema.type == ColumnType::STRING) return finish_string(column);
    return finish_numeric(column);
}

void reset(ColumnBuilder& column) {
    column.ints.clear();
    column.doubles.clear();
    column.strings.clear();
    column.valid.clear();
    column.null_count = 0;
}

void widen_to_double(ColumnBuilder& column) {
    for (auto v : column.ints) column.doubles.push_back(static_cast<double>(v));
    column.ints.clear();
}

void widen_to_string(ColumnBuilder& column) {
    if (column.schema.type == ColumnType::DOUBLE) {
        for (auto v : column.doubles) column.strings.push_back(std::to_string(v));
    } else {
        for (auto v : column.ints) column.strings.push_back(std::to_string(v));
    }
    column.ints.clear();
    column.doubles.clear();
}

}

std::shared_ptr<arrow::Schema> arrow_schema_of(const InferredSchema& schema) {
    arrow::FieldVector fields;
    for (const auto& column : schema.columns) {
        fields.push_back(arrow::field(column.name, arrow_type(column.type)));
    }
    return arrow::schema(fields);
}

BatchBuilder make_batch_builder(const InferredSchema& schema) {
    BatchBuilder builder;
    for (const auto& column : schema.columns) builder.columns.push_back({column});
    return builder;
}

std::expected<void, std::string> widen_column(ColumnBuilder& column, ColumnType to) {
    if (to == ColumnType::DOUBLE) widen_to_double(column);
    else if (to == ColumnType::STRING) widen_to_string(column);
    else return std::unexpected("unsupported widening target");
    column.schema.type = to;
    return {};
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

}

BatchSink::BatchSink(BatchBuilder& builder, TypeConflictPolicy policy)
    : builder_(builder), policy_(policy), seen_(builder.columns.size(), 0) {
    for (std::size_t i = 0; i < builder_.columns.size(); ++i) {
        index_.emplace(builder_.columns[i].schema.json_path, i);
    }
}

std::expected<void, std::string> BatchSink::field(std::string_view path, CellValue value) {
    auto it = index_.find(path);
    if (it == index_.end() || seen_[it->second]) return {};
    auto& column = builder_.columns[it->second];
    if (auto ok = ensure_fits(column, value, policy_); !ok) {
        fatal_ = true;
        return ok;
    }
    if (auto ok = append_cell(column, value); !ok) {
        fatal_ = true;
        return ok;
    }
    seen_[it->second] = 1;
    return {};
}

std::expected<void, std::string> BatchSink::end_row() {
    for (std::size_t i = 0; i < builder_.columns.size(); ++i) {
        if (!seen_[i]) push_null(builder_.columns[i]);
        seen_[i] = 0;
    }
    ++builder_.n_rows;
    return {};
}

std::expected<RecordBatch, std::string> build_batch(BatchBuilder& builder) {
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    arrow::FieldVector fields;
    for (auto& column : builder.columns) {
        auto array = finish_array(column);
        if (!array) return std::unexpected(array.error());
        arrays.push_back(*array);
        fields.push_back(arrow::field(column.schema.name, arrow_type(column.schema.type)));
        reset(column);
    }
    auto data = arrow::RecordBatch::Make(arrow::schema(fields), builder.n_rows, arrays);
    return RecordBatch{data, std::exchange(builder.n_rows, 0)};
}

}
