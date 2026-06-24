#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "riffle/constants.hpp"

namespace riffle {

using CellValue = std::variant<std::monostate, std::int64_t, double, bool, std::string_view>;

enum class ColumnType { INT64, DOUBLE, BOOL, STRING, TIMESTAMP, NULLTYPE };

enum class OnError { SKIP, ABORT, COLLECT };

enum class TypeConflictPolicy { WIDEN, STRING, ERROR };

enum class CompressionCodec { NONE, SNAPPY, ZSTD };

enum class OutputFormat { PARQUET, COLUMNAR_RAW };

enum class PipelineState { INIT, INFER_SCHEMA, CONVERT, FLUSH, FINALIZE, DONE, ABORTED };

std::string_view to_string(ColumnType type);
std::string_view to_string(OnError value);
std::string_view to_string(TypeConflictPolicy value);
std::string_view to_string(CompressionCodec value);
std::string_view to_string(OutputFormat value);
std::string_view to_string(PipelineState value);

std::expected<ColumnType, std::string> parse_column_type(std::string_view text);
std::expected<OnError, std::string> parse_on_error(std::string_view text);
std::expected<TypeConflictPolicy, std::string> parse_type_conflict_policy(std::string_view text);
std::expected<CompressionCodec, std::string> parse_compression_codec(std::string_view text);
std::expected<OutputFormat, std::string> parse_output_format(std::string_view text);

ColumnType column_type_of(const CellValue& value);

struct ColumnSchema {
    std::string name;
    ColumnType type{};
    bool nullable = true;
    std::string json_path;
};

struct InferredSchema {
    std::vector<ColumnSchema> columns;
    std::size_t sampled_rows = 0;
    bool had_conflicts = false;
};

struct Projection {
    std::vector<std::string> select;
    std::vector<std::string> exclude;
    std::vector<std::pair<std::string, std::string>> rename;
};

struct Config {
    std::vector<std::string> inputs;
    std::string output_path;
    OutputFormat output_format = OutputFormat::PARQUET;
    InferredSchema schema_override;
    Projection projection;
    CompressionCodec compression = CompressionCodec::ZSTD;
    std::size_t batch_rows = DEFAULT_BATCH_ROWS;
    std::size_t batch_bytes = MAX_BATCH_BYTES;
    OnError on_error = OnError::SKIP;
    TypeConflictPolicy type_conflict = TypeConflictPolicy::WIDEN;
    bool emit_stats = false;
    bool print_schema = false;
};

struct ParseError {
    std::size_t line_no = 0;
    std::string reason;
    std::string raw;
};

struct ConvertStats {
    std::size_t rows_read = 0;
    std::size_t rows_written = 0;
    std::size_t rows_skipped = 0;
    std::size_t bytes_in = 0;
    std::size_t bytes_out = 0;
    std::uint64_t elapsed_ms = 0;
    PipelineState final_state = PipelineState::INIT;
    std::vector<ParseError> errors;
};

}
