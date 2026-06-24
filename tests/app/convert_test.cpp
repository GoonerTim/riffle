#include "riffle/convert.hpp"

#include <arrow/api.h>
#include <arrow/io/compressed.h>
#include <arrow/io/file.h>
#include <arrow/util/compression.h>
#include <arrow/util/config.h>
#include <gtest/gtest.h>
#include <parquet/arrow/reader.h>

#include <cstdint>
#include <fstream>
#include <string>

#include "riffle/factories.hpp"
#include "riffle/types.hpp"

namespace riffle {
namespace {

std::string write_file(const std::string& name, const std::string& body) {
    std::string path = ::testing::TempDir() + name;
    std::ofstream(path) << body;
    return path;
}

std::shared_ptr<arrow::Table> read_parquet(const std::string& path) {
    auto infile = arrow::io::ReadableFile::Open(path).ValueOrDie();
    std::shared_ptr<arrow::Table> table;
#if ARROW_VERSION_MAJOR >= 19
    auto reader = parquet::arrow::OpenFile(infile, arrow::default_memory_pool()).ValueOrDie();
    table = reader->ReadTable().ValueOrDie();
#else
    std::unique_ptr<parquet::arrow::FileReader> reader;
    (void)parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader);
    (void)reader->ReadTable(&table);
#endif
    return table;
}

Config config_for(const std::string& in, const std::string& out) {
    return make_Config({.inputs = {in}, .output_path = out});
}

std::string write_gzip(const std::string& name, const std::string& body) {
    std::string path = ::testing::TempDir() + name;
    auto sink = arrow::io::FileOutputStream::Open(path).ValueOrDie();
    auto codec = arrow::util::Codec::Create(arrow::Compression::GZIP).ValueOrDie();
    auto out = arrow::io::CompressedOutputStream::Make(codec.get(), sink).ValueOrDie();
    (void)out->Write(body.data(), static_cast<std::int64_t>(body.size()));
    (void)out->Close();
    return path;
}

}

TEST(Convert, AppliesSelectAndRename) {
    auto in = write_file("c_proj.jsonl", "{\"a\":1,\"b\":\"x\",\"c\":2.5}\n");
    auto out = ::testing::TempDir() + "c_proj.parquet";
    auto cfg = config_for(in, out);
    cfg.projection.select = {"c", "a"};
    cfg.projection.rename = {{"a", "id"}};
    auto stats = convert(cfg);
    EXPECT_EQ(stats.final_state, PipelineState::DONE);
    auto table = read_parquet(out);
    ASSERT_EQ(table->num_columns(), 2);
    EXPECT_EQ(table->schema()->field(0)->name(), "c");
    EXPECT_EQ(table->schema()->field(1)->name(), "id");
}

TEST(Convert, MultiThreadedMatchesSingleThreadedOutput) {
    std::string body;
    const int rows = 2000;
    for (int i = 0; i < rows; ++i) {
        body += R"({"id":)" + std::to_string(i) + R"(,"level":")" + (i % 2 ? "info" : "error") +
                R"(","v":)" + std::to_string(i * 0.5) + "}\n";
    }
    auto in = write_file("c_mt.jsonl", body);
    auto out1 = ::testing::TempDir() + "c_mt1.parquet";
    auto out4 = ::testing::TempDir() + "c_mt4.parquet";

    auto cfg1 = config_for(in, out1);
    cfg1.batch_rows = 128;
    auto cfg4 = config_for(in, out4);
    cfg4.batch_rows = 128;
    cfg4.threads = 4;

    auto s1 = convert(cfg1);
    auto s4 = convert(cfg4);
    EXPECT_EQ(s1.final_state, PipelineState::DONE);
    EXPECT_EQ(s4.final_state, PipelineState::DONE);
    EXPECT_EQ(s4.rows_written, static_cast<std::size_t>(rows));
    EXPECT_EQ(s1.rows_written, s4.rows_written);

    auto t1 = read_parquet(out1);
    auto t4 = read_parquet(out4);
    ASSERT_NE(t1, nullptr);
    ASSERT_NE(t4, nullptr);
    EXPECT_TRUE(t1->Equals(*t4));
}

TEST(Convert, MultiThreadedSkipsMalformedLines) {
    std::string body;
    for (int i = 0; i < 500; ++i) body += "{\"v\":" + std::to_string(i) + "}\n";
    body += "broken\n";
    for (int i = 500; i < 1000; ++i) body += "{\"v\":" + std::to_string(i) + "}\n";
    auto in = write_file("c_mtskip.jsonl", body);
    auto out = ::testing::TempDir() + "c_mtskip.parquet";
    auto cfg = config_for(in, out);
    cfg.batch_rows = 64;
    cfg.threads = 3;
    auto stats = convert(cfg);
    EXPECT_EQ(stats.final_state, PipelineState::DONE);
    EXPECT_EQ(stats.rows_written, 1000u);
    EXPECT_EQ(stats.rows_skipped, 1u);
}

TEST(Convert, ReadsGzipInputTransparently) {
    auto in = write_gzip("c_gz.jsonl.gz", "{\"code\":200}\n{\"code\":404}\n{\"code\":500}\n");
    auto out = ::testing::TempDir() + "c_gz.parquet";
    auto stats = convert(config_for(in, out));
    EXPECT_EQ(stats.final_state, PipelineState::DONE);
    EXPECT_EQ(stats.rows_written, 3u);
    EXPECT_EQ(read_parquet(out)->num_rows(), 3);
}

TEST(Convert, FlushesByByteLimitKeepingAllRows) {
    std::string body;
    const int rows = 200;
    for (int i = 0; i < rows; ++i) body += "{\"v\":" + std::to_string(i) + "}\n";
    auto in = write_file("c_bytes.jsonl", body);
    auto out = ::testing::TempDir() + "c_bytes.parquet";
    auto cfg = config_for(in, out);
    cfg.batch_bytes = 16;  // force a flush almost every row
    auto stats = convert(cfg);
    EXPECT_EQ(stats.final_state, PipelineState::DONE);
    EXPECT_EQ(stats.rows_written, static_cast<std::size_t>(rows));
    EXPECT_EQ(read_parquet(out)->num_rows(), rows);
}

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

TEST(Convert, SchemaOverrideForcesColumnType) {
    auto in = write_file("c_ov.jsonl", "{\"code\":200}\n{\"code\":404}\n");
    auto out = ::testing::TempDir() + "c_ov.parquet";
    auto cfg = config_for(in, out);
    cfg.schema_override.columns = {{"code", ColumnType::STRING, true, "code"}};
    auto stats = convert(cfg);
    EXPECT_EQ(stats.final_state, PipelineState::DONE);
    auto table = read_parquet(out);
    EXPECT_EQ(table->schema()->field(0)->type()->id(), arrow::Type::STRING);
}

TEST(Convert, WritesEveryRowWhenInputExceedsSample) {
    std::string body;
    const int rows = 10005;
    for (int i = 0; i < rows; ++i) body += "{\"v\":" + std::to_string(i) + "}\n";
    auto in = write_file("c_count.jsonl", body);
    auto out = ::testing::TempDir() + "c_count.parquet";
    auto stats = convert(config_for(in, out));
    EXPECT_EQ(stats.rows_read, static_cast<std::size_t>(rows));
    EXPECT_EQ(stats.rows_written, static_cast<std::size_t>(rows));
    EXPECT_EQ(read_parquet(out)->num_rows(), rows);
}

TEST(Convert, AutoWidensIntToDoubleBeyondSample) {
    std::string body;
    for (int i = 0; i < 10001; ++i) body += "{\"v\":1}\n";
    body += "{\"v\":2.5}\n";
    auto in = write_file("c_widen.jsonl", body);
    auto out = ::testing::TempDir() + "c_widen.parquet";
    auto stats = convert(config_for(in, out));
    EXPECT_EQ(stats.final_state, PipelineState::DONE);
    auto table = read_parquet(out);
    EXPECT_EQ(table->schema()->field(0)->type()->id(), arrow::Type::DOUBLE);
}

TEST(Convert, AbortsOnBadLineWhenPolicyIsAbort) {
    auto in = write_file("c_abort.jsonl", "{\"a\":1}\nbroken\n");
    auto out = ::testing::TempDir() + "c_abort.parquet";
    auto cfg = make_Config({.inputs = {in}, .output_path = out, .on_error = OnError::ABORT});
    auto stats = convert(cfg);
    EXPECT_EQ(stats.final_state, PipelineState::ABORTED);
}

}
