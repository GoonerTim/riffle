#include "riffle/args.hpp"
#include "riffle/types.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace riffle {
namespace {

std::expected<Config, std::string> parse(std::vector<std::string> argv) {
    return parse_args(argv);
}

}  // namespace

TEST(ParseArgs, ReadsInputAndOutput) {
    auto cfg = parse({"in.jsonl", "-o", "out.parquet"});
    ASSERT_TRUE(cfg.has_value());
    ASSERT_EQ(cfg->inputs.size(), 1u);
    EXPECT_EQ(cfg->inputs[0], "in.jsonl");
    EXPECT_EQ(cfg->output_path, "out.parquet");
}

TEST(ParseArgs, ReadsCompressionAndOnError) {
    auto cfg = parse({"in.jsonl", "-o", "o.parquet", "--compression", "snappy",
                      "--on-error", "collect"});
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

}  // namespace riffle
