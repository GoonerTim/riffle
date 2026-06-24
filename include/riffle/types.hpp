#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace riffle {

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

}  // namespace riffle
