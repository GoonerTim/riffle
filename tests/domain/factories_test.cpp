#include "riffle/constants.hpp"
#include "riffle/factories.hpp"
#include "riffle/types.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace riffle {

TEST(MakeColumnSchema, DefaultsJsonPathToName) {
    auto col = make_ColumnSchema({.name = "code", .type = ColumnType::INT64});
    EXPECT_EQ(col.json_path, "code");
    EXPECT_TRUE(col.nullable);
}

TEST(MakeColumnSchema, KeepsExplicitJsonPath) {
    auto col = make_ColumnSchema(
        {.name = "code", .type = ColumnType::INT64, .json_path = "req.code"});
    EXPECT_EQ(col.json_path, "req.code");
}

TEST(MakeColumnSchema, RejectsEmptyName) {
    EXPECT_THROW(make_ColumnSchema({.name = "", .type = ColumnType::STRING}),
                 std::invalid_argument);
}

TEST(MakeInferredSchema, AcceptsUniqueNames) {
    InferredSchema draft{.columns = {{"a", ColumnType::INT64, true, "a"},
                                     {"b", ColumnType::STRING, true, "b"}}};
    EXPECT_EQ(make_InferredSchema(draft).columns.size(), 2u);
}

TEST(MakeInferredSchema, RejectsDuplicateNames) {
    InferredSchema draft{.columns = {{"a", ColumnType::INT64, true, "a"},
                                     {"a", ColumnType::STRING, true, "a"}}};
    EXPECT_THROW(make_InferredSchema(draft), std::invalid_argument);
}

TEST(MakeParseError, RejectsZeroLine) {
    EXPECT_THROW(make_ParseError({.line_no = 0, .reason = "x"}), std::invalid_argument);
}

TEST(MakeParseError, RejectsEmptyReason) {
    EXPECT_THROW(make_ParseError({.line_no = 1, .reason = ""}), std::invalid_argument);
}

TEST(MakeConfig, RejectsEmptyInputs) {
    EXPECT_THROW(make_Config({.inputs = {}, .output_path = "o.parquet"}),
                 std::invalid_argument);
}

TEST(MakeConfig, RejectsEmptyOutput) {
    EXPECT_THROW(make_Config({.inputs = {"a.jsonl"}, .output_path = ""}),
                 std::invalid_argument);
}

TEST(MakeConfig, AppliesDefaults) {
    auto cfg = make_Config({.inputs = {"a.jsonl"}, .output_path = "o.parquet"});
    EXPECT_EQ(cfg.compression, CompressionCodec::ZSTD);
    EXPECT_EQ(cfg.batch_rows, DEFAULT_BATCH_ROWS);
    EXPECT_EQ(cfg.on_error, OnError::SKIP);
}

TEST(MakeConvertStats, StartsZeroedInInit) {
    auto stats = make_ConvertStats();
    EXPECT_EQ(stats.rows_read, 0u);
    EXPECT_EQ(stats.final_state, PipelineState::INIT);
}

}  // namespace riffle
