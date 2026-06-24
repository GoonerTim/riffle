#include "riffle/types.hpp"

#include <gtest/gtest.h>

namespace riffle {

TEST(ColumnTypeSerde, ToStringCoversAllValues) {
    EXPECT_EQ(to_string(ColumnType::INT64), "int64");
    EXPECT_EQ(to_string(ColumnType::DOUBLE), "double");
    EXPECT_EQ(to_string(ColumnType::BOOL), "bool");
    EXPECT_EQ(to_string(ColumnType::STRING), "string");
    EXPECT_EQ(to_string(ColumnType::TIMESTAMP), "timestamp");
    EXPECT_EQ(to_string(ColumnType::NULLTYPE), "null");
}

TEST(ColumnTypeSerde, ParseRoundTrips) {
    EXPECT_EQ(parse_column_type("double").value(), ColumnType::DOUBLE);
    EXPECT_EQ(parse_column_type("timestamp").value(), ColumnType::TIMESTAMP);
}

TEST(ColumnTypeSerde, ParseRejectsUnknown) {
    EXPECT_FALSE(parse_column_type("bogus").has_value());
}

TEST(OnErrorSerde, RoundTrips) {
    EXPECT_EQ(to_string(OnError::COLLECT), "collect");
    EXPECT_EQ(parse_on_error("abort").value(), OnError::ABORT);
    EXPECT_FALSE(parse_on_error("nope").has_value());
}

TEST(TypeConflictPolicySerde, RoundTrips) {
    EXPECT_EQ(to_string(TypeConflictPolicy::WIDEN), "widen");
    EXPECT_EQ(parse_type_conflict_policy("string").value(), TypeConflictPolicy::STRING);
    EXPECT_FALSE(parse_type_conflict_policy("nope").has_value());
}

TEST(CompressionCodecSerde, RoundTrips) {
    EXPECT_EQ(to_string(CompressionCodec::ZSTD), "zstd");
    EXPECT_EQ(parse_compression_codec("snappy").value(), CompressionCodec::SNAPPY);
    EXPECT_FALSE(parse_compression_codec("nope").has_value());
}

TEST(OutputFormatSerde, RoundTrips) {
    EXPECT_EQ(to_string(OutputFormat::COLUMNAR_RAW), "columnar-raw");
    EXPECT_EQ(parse_output_format("parquet").value(), OutputFormat::PARQUET);
    EXPECT_FALSE(parse_output_format("nope").has_value());
}

TEST(PipelineStateSerde, ToStringCoversTerminalStates) {
    EXPECT_EQ(to_string(PipelineState::INIT), "init");
    EXPECT_EQ(to_string(PipelineState::DONE), "done");
    EXPECT_EQ(to_string(PipelineState::ABORTED), "aborted");
}

}  // namespace riffle
