#include "riffle/args.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

#include "riffle/types.hpp"

namespace riffle {
namespace {

std::expected<Config, std::string> parse(std::vector<std::string> argv) {
    return parse_args(argv);
}

}

TEST(ParseArgs, ReadsInputAndOutput) {
    auto cfg = parse({"in.jsonl", "-o", "out.parquet"});
    ASSERT_TRUE(cfg.has_value());
    ASSERT_EQ(cfg->inputs.size(), 1u);
    EXPECT_EQ(cfg->inputs[0], "in.jsonl");
    EXPECT_EQ(cfg->output_path, "out.parquet");
}

TEST(ParseArgs, ReadsCompressionAndOnError) {
    auto cfg =
        parse({"in.jsonl", "-o", "o.parquet", "--compression", "snappy", "--on-error", "collect"});
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->compression, CompressionCodec::SNAPPY);
    EXPECT_EQ(cfg->on_error, OnError::COLLECT);
}

TEST(ParseArgs, ReadsBatchRowsAndStats) {
    auto cfg = parse({"in.jsonl", "-o", "o.parquet", "--batch-rows", "1000", "--stats"});
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->batch_rows, 1000u);
    EXPECT_TRUE(cfg->emit_stats);
}

TEST(ParseArgs, MissingOutputIsError) {
    EXPECT_FALSE(parse({"in.jsonl"}).has_value());
}

TEST(ParseArgs, UnknownOptionIsError) {
    EXPECT_FALSE(parse({"in.jsonl", "-o", "o.parquet", "--bogus"}).has_value());
}

TEST(ParseArgs, RejectsBadEnumValue) {
    EXPECT_FALSE(parse({"in.jsonl", "-o", "o.parquet", "--compression", "lz9"}).has_value());
}

TEST(ParseArgs, LoadsSchemaOverrideFromFile) {
    std::string path = ::testing::TempDir() + "schema_arg.json";
    std::ofstream(path) << R"({"columns":[{"name":"id","type":"string"}]})";
    auto cfg = parse({"in.jsonl", "-o", "o.parquet", "--schema", path});
    ASSERT_TRUE(cfg.has_value());
    ASSERT_EQ(cfg->schema_override.columns.size(), 1u);
    EXPECT_EQ(cfg->schema_override.columns[0].type, ColumnType::STRING);
}

TEST(ParseArgs, RejectsMissingSchemaFile) {
    EXPECT_FALSE(parse({"in.jsonl", "-o", "o.parquet", "--schema", "/no/such.json"}).has_value());
}

TEST(ParseArgs, ParsesProjectionFlags) {
    auto cfg = parse({"in.jsonl", "-o", "o.parquet", "--select", "a,b", "--exclude", "c",
                      "--rename", "a=id,b=name"});
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->projection.select, (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(cfg->projection.exclude, (std::vector<std::string>{"c"}));
    ASSERT_EQ(cfg->projection.rename.size(), 2u);
    EXPECT_EQ(cfg->projection.rename[0].first, "a");
    EXPECT_EQ(cfg->projection.rename[0].second, "id");
}

TEST(ParseArgs, PrintSchemaAllowsMissingOutput) {
    auto cfg = parse({"in.jsonl", "--print-schema"});
    ASSERT_TRUE(cfg.has_value());
    EXPECT_TRUE(cfg->print_schema);
}

TEST(ParseArgs, RejectsBadRename) {
    EXPECT_FALSE(parse({"in.jsonl", "-o", "o.parquet", "--rename", "noequalssign"}).has_value());
}

}
