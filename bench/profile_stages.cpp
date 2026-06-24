#include <chrono>
#include <cstdio>
#include <expected>
#include <fstream>
#include <string>
#include <string_view>

#include "riffle/batch.hpp"
#include "riffle/json_parser.hpp"
#include "riffle/ports.hpp"
#include "riffle/reader.hpp"
#include "riffle/types.hpp"

using Clock = std::chrono::steady_clock;
using namespace riffle;

static double ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

namespace {
struct CountSink : RowSink {
    std::size_t fields = 0;
    void begin_row() override {}
    std::expected<void, std::string> field(std::string_view, CellValue) override {
        ++fields;
        return {};
    }
    std::expected<void, std::string> end_row() override { return {}; }
};

InferredSchema bench_schema() {
    return {.columns = {{"ts", ColumnType::TIMESTAMP, true, "ts"},
                        {"level", ColumnType::STRING, true, "level"},
                        {"path", ColumnType::STRING, true, "path"},
                        {"status", ColumnType::INT64, true, "status"},
                        {"latency_ms", ColumnType::DOUBLE, true, "latency_ms"},
                        {"user_id", ColumnType::INT64, true, "user_id"}}};
}
}

static double time_read(const std::string& path) {
    std::ifstream in(path);
    LineReader reader(in);
    auto start = Clock::now();
    std::size_t n = 0;
    for (auto line = reader.next(); line; line = reader.next()) n += line->size();
    std::fprintf(stderr, "  (read bytes seen: %zu)\n", n);
    return ms(start, Clock::now());
}

static double time_read_parse(const std::string& path) {
    std::ifstream in(path);
    LineReader reader(in);
    JsonParser parser;
    CountSink sink;
    auto start = Clock::now();
    for (auto line = reader.next(); line; line = reader.next()) (void)parser.parse(*line, sink);
    return ms(start, Clock::now());
}

static double time_read_parse_append(const std::string& path) {
    std::ifstream in(path);
    LineReader reader(in);
    JsonParser parser;
    auto builder = make_batch_builder(bench_schema());
    BatchSink sink(builder, TypeConflictPolicy::WIDEN);
    auto start = Clock::now();
    for (auto line = reader.next(); line; line = reader.next()) {
        (void)parser.parse(*line, sink);
        if (builder.n_rows >= 65536) (void)build_batch(builder);
    }
    (void)build_batch(builder);
    return ms(start, Clock::now());
}

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const std::string path = argv[1];
    const double t_read = time_read(path);
    const double t_parse = time_read_parse(path);
    const double t_append = time_read_parse_append(path);
    std::printf("read            : %8.1f ms\n", t_read);
    std::printf("parse           : %8.1f ms (delta %.1f)\n", t_parse, t_parse - t_read);
    std::printf("append + build  : %8.1f ms (delta %.1f)\n", t_append, t_append - t_parse);
    return 0;
}
