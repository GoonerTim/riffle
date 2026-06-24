#include "riffle/batch.hpp"
#include "riffle/types.hpp"

#include <arrow/api.h>
#include <gtest/gtest.h>

#include <cstdint>

namespace riffle {
namespace {

InferredSchema schema_ab() {
    return {.columns = {{"a", ColumnType::INT64, true, "a"},
                        {"b", ColumnType::STRING, true, "b"}}};
}

Row row_ab(std::int64_t a, std::string b) {
    return Row{{{"a", CellValue{a}}, {"b", CellValue{std::move(b)}}}};
}

}  // namespace

TEST(Batch, BuildsColumnsFromRows) {
    auto builder = make_batch_builder(schema_ab());
    ASSERT_TRUE(append_row(builder, row_ab(1, "x")).has_value());
    ASSERT_TRUE(append_row(builder, row_ab(2, "y")).has_value());
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
    ASSERT_TRUE(append_row(builder, Row{{{"a", CellValue{std::int64_t{5}}}}}).has_value());
    auto batch = build_batch(builder);
    ASSERT_TRUE(batch.has_value());
    auto b = std::static_pointer_cast<arrow::StringArray>(batch->data->column(1));
    EXPECT_TRUE(b->IsNull(0));
}

TEST(Batch, TimestampColumnParsesIso) {
    InferredSchema schema{.columns = {{"ts", ColumnType::TIMESTAMP, true, "ts"}}};
    auto builder = make_batch_builder(schema);
    Row row{{{"ts", CellValue{std::string("1970-01-01T00:00:01Z")}}}};
    ASSERT_TRUE(append_row(builder, row).has_value());
    auto batch = build_batch(builder);
    ASSERT_TRUE(batch.has_value());
    auto ts = std::static_pointer_cast<arrow::TimestampArray>(batch->data->column(0));
    EXPECT_EQ(ts->Value(0), 1000000);
}

TEST(Batch, AutoWidensIntColumnWhenDoubleArrives) {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    auto builder = make_batch_builder(schema);
    ASSERT_TRUE(append_row(builder, Row{{{"v", CellValue{std::int64_t{1}}}}},
                           TypeConflictPolicy::WIDEN)
                    .has_value());
    ASSERT_TRUE(append_row(builder, Row{{{"v", CellValue{2.5}}}}, TypeConflictPolicy::WIDEN)
                    .has_value());
    auto batch = build_batch(builder);
    ASSERT_TRUE(batch.has_value());
    auto v = std::static_pointer_cast<arrow::DoubleArray>(batch->data->column(0));
    EXPECT_DOUBLE_EQ(v->Value(0), 1.0);
    EXPECT_DOUBLE_EQ(v->Value(1), 2.5);
}

TEST(Batch, AutoWidensIncompatibleToString) {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    auto builder = make_batch_builder(schema);
    ASSERT_TRUE(append_row(builder, Row{{{"v", CellValue{std::int64_t{7}}}}},
                           TypeConflictPolicy::WIDEN)
                    .has_value());
    ASSERT_TRUE(append_row(builder, Row{{{"v", CellValue{std::string("hi")}}}},
                           TypeConflictPolicy::WIDEN)
                    .has_value());
    auto batch = build_batch(builder);
    ASSERT_TRUE(batch.has_value());
    auto v = std::static_pointer_cast<arrow::StringArray>(batch->data->column(0));
    EXPECT_EQ(v->GetString(0), "7");
    EXPECT_EQ(v->GetString(1), "hi");
}

TEST(Batch, ErrorPolicyRejectsWidening) {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    auto builder = make_batch_builder(schema);
    ASSERT_TRUE(append_row(builder, Row{{{"v", CellValue{std::int64_t{1}}}}},
                           TypeConflictPolicy::ERROR)
                    .has_value());
    EXPECT_FALSE(append_row(builder, Row{{{"v", CellValue{2.5}}}}, TypeConflictPolicy::ERROR)
                     .has_value());
}

TEST(Batch, WidenColumnIntToDouble) {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    auto builder = make_batch_builder(schema);
    ASSERT_TRUE(append_row(builder, Row{{{"v", CellValue{std::int64_t{3}}}}}).has_value());
    ASSERT_TRUE(widen_column(builder.columns[0], ColumnType::DOUBLE).has_value());
    auto batch = build_batch(builder);
    ASSERT_TRUE(batch.has_value());
    auto v = std::static_pointer_cast<arrow::DoubleArray>(batch->data->column(0));
    EXPECT_DOUBLE_EQ(v->Value(0), 3.0);
}

}  // namespace riffle
