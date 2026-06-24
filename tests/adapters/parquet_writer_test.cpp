#include "riffle/batch.hpp"
#include "riffle/factories.hpp"
#include "riffle/types.hpp"
#include "riffle/writer.hpp"

#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/reader.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

namespace riffle {
namespace {

std::shared_ptr<arrow::Table> read_parquet(const std::string& path) {
    auto infile = arrow::io::ReadableFile::Open(path).ValueOrDie();
    std::unique_ptr<parquet::arrow::FileReader> reader;
    (void)parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader);
    std::shared_ptr<arrow::Table> table;
    (void)reader->ReadTable(&table);
    return table;
}

void put_int(BatchSink& sink, std::int64_t v) {
    sink.begin_row();
    (void)sink.field("v", CellValue{v});
    (void)sink.end_row();
}

RecordBatch one_int_column() {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    auto builder = make_batch_builder(schema);
    BatchSink sink(builder, TypeConflictPolicy::WIDEN);
    put_int(sink, 10);
    put_int(sink, 20);
    return build_batch(builder).value();
}

}  // namespace

TEST(ParquetWriter, RoundTripsBatch) {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    const std::string path = ::testing::TempDir() + "riffle_rt.parquet";
    auto cfg = make_Config({.inputs = {"x"}, .output_path = path});

    auto writer = open_writer(cfg, schema);
    ASSERT_TRUE(writer.has_value());
    ASSERT_TRUE((*writer)->write(one_int_column()).has_value());
    ASSERT_TRUE((*writer)->finish().has_value());

    auto table = read_parquet(path);
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->num_rows(), 2);
    auto col = std::static_pointer_cast<arrow::Int64Array>(table->column(0)->chunk(0));
    EXPECT_EQ(col->Value(0), 10);
    EXPECT_EQ(col->Value(1), 20);
}

}  // namespace riffle
