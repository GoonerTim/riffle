#include "riffle/convert.hpp"
#include "riffle/factories.hpp"
#include "riffle/types.hpp"

#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/reader.h>
#include <gtest/gtest.h>

#include <fstream>
#include <string>

namespace riffle {
namespace {

std::string write_file(const std::string& name, const std::string& body) {
    std::string path = ::testing::TempDir() + name;
    std::ofstream(path) << body;
    return path;
}

std::shared_ptr<arrow::Table> read_parquet(const std::string& path) {
    auto infile = arrow::io::ReadableFile::Open(path).ValueOrDie();
    std::unique_ptr<parquet::arrow::FileReader> reader;
    (void)parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader);
    std::shared_ptr<arrow::Table> table;
    (void)reader->ReadTable(&table);
    return table;
}

Config config_for(const std::string& in, const std::string& out) {
    return make_Config({.inputs = {in}, .output_path = out});
}

}  // namespace

TEST(Convert, WritesParquetFromJsonl) {
    auto in = write_file("c_ok.jsonl", "{\"code\":200}\n{\"code\":404}\n{\"code\":500}\n");
    auto out = ::testing::TempDir() + "c_ok.parquet";
    auto stats = convert(config_for(in, out));
    EXPECT_EQ(stats.final_state, PipelineState::DONE);
    EXPECT_EQ(stats.rows_read, 3u);
    EXPECT_EQ(stats.rows_written, 3u);
    EXPECT_EQ(read_parquet(out)->num_rows(), 3);
}

TEST(Convert, SkipsMalformedLines) {
    auto in = write_file("c_bad.jsonl", "{\"code\":200}\nnot json\n{\"code\":500}\n");
    auto out = ::testing::TempDir() + "c_bad.parquet";
    auto stats = convert(config_for(in, out));
    EXPECT_EQ(stats.final_state, PipelineState::DONE);
    EXPECT_EQ(stats.rows_written, 2u);
    EXPECT_EQ(stats.rows_skipped, 1u);
}

TEST(Convert, CollectsErrorsWhenRequested) {
    auto in = write_file("c_col.jsonl", "{\"a\":1}\nbroken\n");
    auto out = ::testing::TempDir() + "c_col.parquet";
    auto cfg = make_Config({.inputs = {in}, .output_path = out, .on_error = OnError::COLLECT});
    auto stats = convert(cfg);
    ASSERT_EQ(stats.errors.size(), 1u);
    EXPECT_EQ(stats.errors[0].line_no, 2u);
}

TEST(Convert, AbortsOnBadLineWhenPolicyIsAbort) {
    auto in = write_file("c_abort.jsonl", "{\"a\":1}\nbroken\n");
    auto out = ::testing::TempDir() + "c_abort.parquet";
    auto cfg = make_Config({.inputs = {in}, .output_path = out, .on_error = OnError::ABORT});
    auto stats = convert(cfg);
    EXPECT_EQ(stats.final_state, PipelineState::ABORTED);
}

}  // namespace riffle
