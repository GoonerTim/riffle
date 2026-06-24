#include "riffle/ports.hpp"
#include "riffle/schema.hpp"
#include "riffle/types.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace riffle {
namespace {

// Minimal in-memory RowSource for driving infer_schema.
class FakeSource : public RowSource {
public:
    explicit FakeSource(std::vector<Row> rows) : rows_(std::move(rows)) {}
    std::optional<Row> next() override {
        if (index_ >= rows_.size()) return std::nullopt;
        return rows_[index_++];
    }

private:
    std::vector<Row> rows_;
    std::size_t index_ = 0;
};

Field i64(std::string path, std::int64_t v) { return {std::move(path), CellValue{v}}; }
Field dbl(std::string path, double v) { return {std::move(path), CellValue{v}}; }
Field str(std::string path, std::string v) { return {std::move(path), CellValue{std::move(v)}}; }
Field nul(std::string path) { return {std::move(path), CellValue{}}; }

}  // namespace

TEST(InferSchema, ColumnsFollowFirstAppearanceOrder) {
    FakeSource src({Row{{str("ts", "t"), i64("code", 200)}}});
    auto schema = infer_schema(src, TypeConflictPolicy::WIDEN);
    ASSERT_EQ(schema.columns.size(), 2u);
    EXPECT_EQ(schema.columns[0].name, "ts");
    EXPECT_EQ(schema.columns[1].name, "code");
    EXPECT_EQ(schema.columns[1].type, ColumnType::INT64);
}

TEST(InferSchema, WidensTypesAcrossRows) {
    FakeSource src({Row{{i64("v", 1)}}, Row{{dbl("v", 2.5)}}});
    auto schema = infer_schema(src, TypeConflictPolicy::WIDEN);
    ASSERT_EQ(schema.columns.size(), 1u);
    EXPECT_EQ(schema.columns[0].type, ColumnType::DOUBLE);
}

TEST(InferSchema, OnlyNullColumnIsNullType) {
    FakeSource src({Row{{nul("x")}}, Row{{nul("x")}}});
    auto schema = infer_schema(src, TypeConflictPolicy::WIDEN);
    ASSERT_EQ(schema.columns.size(), 1u);
    EXPECT_EQ(schema.columns[0].type, ColumnType::NULLTYPE);
}

TEST(InferSchema, CountsSampledRows) {
    FakeSource src({Row{{i64("v", 1)}}, Row{{i64("v", 2)}}, Row{{i64("v", 3)}}});
    auto schema = infer_schema(src, TypeConflictPolicy::WIDEN);
    EXPECT_EQ(schema.sampled_rows, 3u);
}

}  // namespace riffle
