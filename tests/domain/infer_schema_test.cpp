#include "riffle/schema.hpp"
#include "riffle/types.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

namespace riffle {
namespace {

using FieldList = std::initializer_list<std::pair<std::string_view, CellValue>>;

void feed(InferenceSink& sink, FieldList fields) {
    sink.begin_row();
    for (const auto& [path, value] : fields) (void)sink.field(path, value);
    (void)sink.end_row();
}

CellValue i64(std::int64_t v) { return CellValue{v}; }
CellValue dbl(double v) { return CellValue{v}; }
CellValue str(std::string v) { return CellValue{std::move(v)}; }
CellValue nul() { return CellValue{}; }

}

TEST(InferSchema, ColumnsFollowFirstAppearanceOrder) {
    InferenceSink sink(TypeConflictPolicy::WIDEN);
    feed(sink, {{"ts", str("t")}, {"code", i64(200)}});
    auto schema = sink.schema();
    ASSERT_EQ(schema.columns.size(), 2u);
    EXPECT_EQ(schema.columns[0].name, "ts");
    EXPECT_EQ(schema.columns[1].name, "code");
    EXPECT_EQ(schema.columns[1].type, ColumnType::INT64);
}

TEST(InferSchema, WidensTypesAcrossRows) {
    InferenceSink sink(TypeConflictPolicy::WIDEN);
    feed(sink, {{"v", i64(1)}});
    feed(sink, {{"v", dbl(2.5)}});
    auto schema = sink.schema();
    ASSERT_EQ(schema.columns.size(), 1u);
    EXPECT_EQ(schema.columns[0].type, ColumnType::DOUBLE);
}

TEST(InferSchema, OnlyNullColumnIsNullType) {
    InferenceSink sink(TypeConflictPolicy::WIDEN);
    feed(sink, {{"x", nul()}});
    feed(sink, {{"x", nul()}});
    auto schema = sink.schema();
    ASSERT_EQ(schema.columns.size(), 1u);
    EXPECT_EQ(schema.columns[0].type, ColumnType::NULLTYPE);
}

TEST(InferSchema, CountsSampledRows) {
    InferenceSink sink(TypeConflictPolicy::WIDEN);
    feed(sink, {{"v", i64(1)}});
    feed(sink, {{"v", i64(2)}});
    feed(sink, {{"v", i64(3)}});
    EXPECT_EQ(sink.schema().sampled_rows, 3u);
}

TEST(InferSchema, IsoStringsBecomeTimestamp) {
    InferenceSink sink(TypeConflictPolicy::WIDEN);
    feed(sink, {{"ts", str("2026-06-24T10:00:00Z")}});
    feed(sink, {{"ts", str("2026-06-24T10:00:01Z")}});
    EXPECT_EQ(sink.schema().columns[0].type, ColumnType::TIMESTAMP);
}

TEST(InferSchema, MixedTimestampAndPlainStringIsString) {
    InferenceSink sink(TypeConflictPolicy::WIDEN);
    feed(sink, {{"ts", str("2026-06-24T10:00:00Z")}});
    feed(sink, {{"ts", str("oops")}});
    EXPECT_EQ(sink.schema().columns[0].type, ColumnType::STRING);
}

}
