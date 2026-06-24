#include "riffle/batch.hpp"
#include "riffle/types.hpp"

#include <arrow/api.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

namespace riffle {
namespace {

using FieldList = std::initializer_list<std::pair<std::string_view, CellValue>>;

void feed(BatchSink& sink, FieldList fields) {
    sink.begin_row();
    for (const auto& [path, value] : fields) (void)sink.field(path, value);
    (void)sink.end_row();
}

InferredSchema schema_ab() {
    return {.columns = {{"a", ColumnType::INT64, true, "a"},
                        {"b", ColumnType::STRING, true, "b"}}};
}

CellValue i64(std::int64_t v) { return CellValue{v}; }

}  // namespace

TEST(Batch, BuildsColumnsFromRows) {
    auto builder = make_batch_builder(schema_ab());
    BatchSink sink(builder, TypeConflictPolicy::WIDEN);
    feed(sink, {{"a", i64(1)}, {"b", CellValue{std::string("x")}}});
    feed(sink, {{"a", i64(2)}, {"b", CellValue{std::string("y")}}});
    auto batch = build_batch(builder);
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->n_rows, 2u);
    auto a = std::static_pointer_cast<arrow::Int64Array>(batch->data->column(0));
    EXPECT_EQ(a->Value(0), 1);
    EXPECT_EQ(a->Value(1), 2);
    auto b = std::static_pointer_cast<arrow::StringArray>(batch->data->column(1));
    EXPECT_EQ(b->GetString(0), "x");
}

TEST(Batch, MissingFieldBecomesNull) {
    auto builder = make_batch_builder(schema_ab());
    BatchSink sink(builder, TypeConflictPolicy::WIDEN);
    feed(sink, {{"a", i64(5)}});
    auto batch = build_batch(builder);
    ASSERT_TRUE(batch.has_value());
    auto b = std::static_pointer_cast<arrow::StringArray>(batch->data->column(1));
    EXPECT_TRUE(b->IsNull(0));
}

TEST(Batch, TimestampColumnParsesIso) {
    InferredSchema schema{.columns = {{"ts", ColumnType::TIMESTAMP, true, "ts"}}};
    auto builder = make_batch_builder(schema);
    BatchSink sink(builder, TypeConflictPolicy::WIDEN);
    feed(sink, {{"ts", CellValue{std::string("1970-01-01T00:00:01Z")}}});
    auto batch = build_batch(builder);
    ASSERT_TRUE(batch.has_value());
    auto ts = std::static_pointer_cast<arrow::TimestampArray>(batch->data->column(0));
    EXPECT_EQ(ts->Value(0), 1000000);
}

TEST(Batch, AutoWidensIntColumnWhenDoubleArrives) {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    auto builder = make_batch_builder(schema);
    BatchSink sink(builder, TypeConflictPolicy::WIDEN);
    feed(sink, {{"v", i64(1)}});
    feed(sink, {{"v", CellValue{2.5}}});
    auto batch = build_batch(builder);
    ASSERT_TRUE(batch.has_value());
    auto v = std::static_pointer_cast<arrow::DoubleArray>(batch->data->column(0));
    EXPECT_DOUBLE_EQ(v->Value(0), 1.0);
    EXPECT_DOUBLE_EQ(v->Value(1), 2.5);
}

TEST(Batch, AutoWidensIncompatibleToString) {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    auto builder = make_batch_builder(schema);
    BatchSink sink(builder, TypeConflictPolicy::WIDEN);
    feed(sink, {{"v", i64(7)}});
    feed(sink, {{"v", CellValue{std::string("hi")}}});
    auto batch = build_batch(builder);
    ASSERT_TRUE(batch.has_value());
    auto v = std::static_pointer_cast<arrow::StringArray>(batch->data->column(0));
    EXPECT_EQ(v->GetString(0), "7");
    EXPECT_EQ(v->GetString(1), "hi");
}

TEST(Batch, ErrorPolicyRejectsWidening) {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    auto builder = make_batch_builder(schema);
    BatchSink sink(builder, TypeConflictPolicy::ERROR);
    feed(sink, {{"v", i64(1)}});
    sink.begin_row();
    EXPECT_FALSE(sink.field("v", CellValue{2.5}).has_value());
    EXPECT_TRUE(sink.fatal());
}

TEST(Batch, WidenColumnIntToDouble) {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    auto builder = make_batch_builder(schema);
    BatchSink sink(builder, TypeConflictPolicy::WIDEN);
    feed(sink, {{"v", i64(3)}});
    ASSERT_TRUE(widen_column(builder.columns[0], ColumnType::DOUBLE).has_value());
    auto batch = build_batch(builder);
    ASSERT_TRUE(batch.has_value());
    auto v = std::static_pointer_cast<arrow::DoubleArray>(batch->data->column(0));
    EXPECT_DOUBLE_EQ(v->Value(0), 3.0);
}

}  // namespace riffle
