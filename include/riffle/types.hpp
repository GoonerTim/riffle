#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "riffle/constants.hpp"

namespace riffle {

// A single JSON scalar value; monostate represents null/absent.
using CellValue = std::variant<std::monostate, std::int64_t, double, bool, std::string>;

// One field of a parsed row: flattened path + its value.
struct Field {
    std::string path;
    CellValue value;
};

// A parsed input line as an ordered list of fields.
struct Row {
    std::vector<Field> fields;
};

// Logical type a JSON value maps to in the output schema.
enum class ColumnType { INT64, DOUBLE, BOOL, STRING, TIMESTAMP, NULLTYPE };

// Reaction to a malformed input line.
enum class OnError { SKIP, ABORT, COLLECT };

// How a column's conflicting observed types are resolved.
enum class TypeConflictPolicy { WIDEN, STRING, ERROR };

// Parquet page compression codec.
enum class CompressionCodec { NONE, SNAPPY, ZSTD };

// Output file format.
enum class OutputFormat { PARQUET, COLUMNAR_RAW };

// Conversion pipeline state machine.
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

// Logical type a cell value carries (monostate → NULLTYPE).
ColumnType column_type_of(const CellValue& value);

// ---------------------------------------------------------------------------
// Value structures (immutable data; constructed via make_* factories).
// ---------------------------------------------------------------------------

// One output column.
struct ColumnSchema {
    std::string name;
    ColumnType type{};
    bool nullable = true;
    std::string json_path;  // defaults to name
};

// Ordered set of columns plus inference diagnostics.
struct InferredSchema {
    std::vector<ColumnSchema> columns;
    std::size_t sampled_rows = 0;
    bool had_conflicts = false;
};

// Parameters of one conversion run.
struct Config {
    std::vector<std::string> inputs;
    std::string output_path;
    OutputFormat output_format = OutputFormat::PARQUET;
    InferredSchema schema_override;  // empty → infer
    CompressionCodec compression = CompressionCodec::ZSTD;
    std::size_t batch_rows = DEFAULT_BATCH_ROWS;
    OnError on_error = OnError::SKIP;
    TypeConflictPolicy type_conflict = TypeConflictPolicy::WIDEN;
    bool emit_stats = false;
};

// One rejected input line (collected under OnError::COLLECT).
struct ParseError {
    std::size_t line_no = 0;
    std::string reason;
    std::string raw;
};

// Outcome of a conversion run.
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

}  // namespace riffle
