#include "riffle/types.hpp"

#include <array>
#include <span>
#include <utility>

namespace riffle {
namespace {

template <class E>
using Row = std::pair<E, std::string_view>;

template <class E>
std::string_view name_of(std::span<const Row<E>> table, E value) {
    for (const auto& [entry, name] : table) {
        if (entry == value) return name;
    }
    return {};
}

template <class E>
std::expected<E, std::string> value_of(std::span<const Row<E>> table, std::string_view text) {
    for (const auto& [entry, name] : table) {
        if (name == text) return entry;
    }
    return std::unexpected("unknown value: " + std::string(text));
}

constexpr std::array<Row<ColumnType>, 6> kColumnTypes{{
    {ColumnType::INT64, "int64"},
    {ColumnType::DOUBLE, "double"},
    {ColumnType::BOOL, "bool"},
    {ColumnType::STRING, "string"},
    {ColumnType::TIMESTAMP, "timestamp"},
    {ColumnType::NULLTYPE, "null"},
}};

constexpr std::array<Row<OnError>, 3> kOnError{{
    {OnError::SKIP, "skip"},
    {OnError::ABORT, "abort"},
    {OnError::COLLECT, "collect"},
}};

constexpr std::array<Row<TypeConflictPolicy>, 3> kConflictPolicies{{
    {TypeConflictPolicy::WIDEN, "widen"},
    {TypeConflictPolicy::STRING, "string"},
    {TypeConflictPolicy::ERROR, "error"},
}};

constexpr std::array<Row<CompressionCodec>, 3> kCodecs{{
    {CompressionCodec::NONE, "none"},
    {CompressionCodec::SNAPPY, "snappy"},
    {CompressionCodec::ZSTD, "zstd"},
}};

constexpr std::array<Row<OutputFormat>, 2> kFormats{{
    {OutputFormat::PARQUET, "parquet"},
    {OutputFormat::COLUMNAR_RAW, "columnar-raw"},
}};

constexpr std::array<Row<PipelineState>, 7> kStates{{
    {PipelineState::INIT, "init"},
    {PipelineState::INFER_SCHEMA, "infer_schema"},
    {PipelineState::CONVERT, "convert"},
    {PipelineState::FLUSH, "flush"},
    {PipelineState::FINALIZE, "finalize"},
    {PipelineState::DONE, "done"},
    {PipelineState::ABORTED, "aborted"},
}};

}

std::string_view to_string(ColumnType type) {
    return name_of<ColumnType>(kColumnTypes, type);
}
std::string_view to_string(OnError value) {
    return name_of<OnError>(kOnError, value);
}
std::string_view to_string(TypeConflictPolicy v) {
    return name_of<TypeConflictPolicy>(kConflictPolicies, v);
}
std::string_view to_string(CompressionCodec v) {
    return name_of<CompressionCodec>(kCodecs, v);
}
std::string_view to_string(OutputFormat value) {
    return name_of<OutputFormat>(kFormats, value);
}
std::string_view to_string(PipelineState value) {
    return name_of<PipelineState>(kStates, value);
}

std::expected<ColumnType, std::string> parse_column_type(std::string_view text) {
    return value_of<ColumnType>(kColumnTypes, text);
}
std::expected<OnError, std::string> parse_on_error(std::string_view text) {
    return value_of<OnError>(kOnError, text);
}
std::expected<TypeConflictPolicy, std::string> parse_type_conflict_policy(std::string_view text) {
    return value_of<TypeConflictPolicy>(kConflictPolicies, text);
}
std::expected<CompressionCodec, std::string> parse_compression_codec(std::string_view text) {
    return value_of<CompressionCodec>(kCodecs, text);
}
std::expected<OutputFormat, std::string> parse_output_format(std::string_view text) {
    return value_of<OutputFormat>(kFormats, text);
}

ColumnType column_type_of(const CellValue& value) {
    switch (value.index()) {
        case 1:
            return ColumnType::INT64;
        case 2:
            return ColumnType::DOUBLE;
        case 3:
            return ColumnType::BOOL;
        case 4:
            return ColumnType::STRING;
        default:
            return ColumnType::NULLTYPE;
    }
}

}
