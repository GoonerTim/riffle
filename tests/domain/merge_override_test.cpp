#include "riffle/schema.hpp"
#include "riffle/types.hpp"

#include <gtest/gtest.h>

namespace riffle {
namespace {

ColumnSchema col(std::string name, ColumnType type) {
    return {std::move(name), type, true, {}};
}

}  // namespace

TEST(MergeOverride, ReplacesExistingColumnType) {
    InferredSchema inferred{.columns = {col("id", ColumnType::INT64)}};
    InferredSchema override{.columns = {col("id", ColumnType::STRING)}};
    auto merged = merge_override(inferred, override);
    ASSERT_EQ(merged.columns.size(), 1u);
    EXPECT_EQ(merged.columns[0].type, ColumnType::STRING);
}

TEST(MergeOverride, AppendsUnknownColumn) {
    InferredSchema inferred{.columns = {col("a", ColumnType::INT64)}};
    InferredSchema override{.columns = {col("b", ColumnType::STRING)}};
    auto merged = merge_override(inferred, override);
    ASSERT_EQ(merged.columns.size(), 2u);
    EXPECT_EQ(merged.columns[1].name, "b");
}

TEST(MergeOverride, LeavesUnreferencedColumnsUntouched) {
    InferredSchema inferred{.columns = {col("a", ColumnType::INT64), col("b", ColumnType::BOOL)}};
    InferredSchema override{.columns = {col("a", ColumnType::STRING)}};
    auto merged = merge_override(inferred, override);
    EXPECT_EQ(merged.columns[1].type, ColumnType::BOOL);
}

}  // namespace riffle
