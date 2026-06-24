#include "riffle/convert.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "riffle/batch.hpp"
#include "riffle/factories.hpp"
#include "riffle/json_parser.hpp"
#include "riffle/reader.hpp"
#include "riffle/schema.hpp"
#include "riffle/writer.hpp"

namespace riffle {
namespace {

using Clock = std::chrono::steady_clock;

std::unique_ptr<std::istream> open_stream(const std::string& path) {
    if (path == "-") return std::make_unique<std::istream>(std::cin.rdbuf());
    return std::make_unique<std::ifstream>(path);
}

class ChainedLines {
public:
    explicit ChainedLines(std::vector<std::string> paths) : paths_(std::move(paths)) { advance(); }

    std::optional<std::string_view> next() {
        while (reader_) {
            if (auto line = reader_->next()) return line;
            advance();
        }
        return std::nullopt;
    }

private:
    void advance() {
        reader_.reset();
        stream_.reset();
        if (index_ >= paths_.size()) return;
        stream_ = open_stream(paths_[index_++]);
        reader_ = std::make_unique<LineReader>(*stream_);
    }

    std::vector<std::string> paths_;
    std::size_t index_ = 0;
    std::unique_ptr<std::istream> stream_;
    std::unique_ptr<LineReader> reader_;
};

std::vector<std::string> read_sample(ChainedLines& lines, std::size_t limit) {
    std::vector<std::string> sample;
    while (sample.size() < limit) {
        auto line = lines.next();
        if (!line) break;
        sample.emplace_back(*line);
    }
    return sample;
}

InferredSchema infer(const Config& cfg, const std::vector<std::string>& sample) {
    JsonParser parser;
    InferenceSink sink(cfg.type_conflict);
    for (const auto& line : sample) (void)parser.parse(line, sink);
    auto schema = sink.schema();
    if (cfg.schema_override.columns.empty()) return schema;
    return merge_override(schema, cfg.schema_override);
}

std::expected<InferredSchema, std::string> resolve_schema(const Config& cfg,
                                                          const std::vector<std::string>& sample) {
    return apply_projection(infer(cfg, sample), cfg.projection);
}

struct Ctx {
    const Config& cfg;
    JsonParser& parser;
    BatchSink& sink;
    BatchBuilder& builder;
    std::unique_ptr<Writer> writer;
    ConvertStats stats;
    std::size_t line_no = 0;
};

InferredSchema current_schema(const BatchBuilder& builder) {
    InferredSchema schema;
    for (const auto& column : builder.columns) schema.columns.push_back(column.schema);
    return schema;
}

std::expected<void, std::string> ensure_writer(Ctx& ctx) {
    if (ctx.writer) return {};
    auto writer = open_writer(ctx.cfg, current_schema(ctx.builder));
    if (!writer) return std::unexpected(writer.error());
    ctx.writer = std::move(*writer);
    return {};
}

std::expected<void, std::string> flush(Ctx& ctx) {
    if (ctx.builder.n_rows == 0) return {};
    if (auto ok = ensure_writer(ctx); !ok) return ok;
    auto batch = build_batch(ctx.builder);
    if (!batch) return std::unexpected(batch.error());
    ctx.stats.rows_written += batch->n_rows;
    return ctx.writer->write(*batch);
}

std::expected<void, std::string> on_bad_line(Ctx& ctx, std::string_view line, std::string reason) {
    if (ctx.sink.fatal() || ctx.cfg.on_error == OnError::ABORT) return std::unexpected(reason);
    ++ctx.stats.rows_skipped;
    if (ctx.cfg.on_error == OnError::COLLECT) {
        ctx.stats.errors.push_back(make_ParseError(
            {.line_no = ctx.line_no, .reason = std::move(reason), .raw = std::string(line)}));
    }
    return {};
}

std::expected<void, std::string> process(Ctx& ctx, std::string_view line) {
    ++ctx.line_no;
    if (auto ok = ctx.parser.parse(line, ctx.sink); !ok) return on_bad_line(ctx, line, ok.error());
    ++ctx.stats.rows_read;
    if (ctx.builder.n_rows >= ctx.cfg.batch_rows) return flush(ctx);
    return {};
}

std::expected<void, std::string> pump(Ctx& ctx, const std::vector<std::string>& sample,
                                      ChainedLines& lines) {
    for (const auto& line : sample) {
        if (auto ok = process(ctx, line); !ok) return ok;
    }
    for (auto line = lines.next(); line; line = lines.next()) {
        if (auto ok = process(ctx, *line); !ok) return ok;
    }
    return flush(ctx);
}

std::expected<void, std::string> finalize(Ctx& ctx) {
    if (auto ok = ensure_writer(ctx); !ok) return ok;
    return ctx.writer->finish();
}

void set_final_state(ConvertStats& stats, const std::expected<void, std::string>& result) {
    stats.final_state = result ? PipelineState::DONE : PipelineState::ABORTED;
}

std::uint64_t elapsed_ms(Clock::time_point start) {
    using std::chrono::duration_cast, std::chrono::milliseconds;
    return static_cast<std::uint64_t>(duration_cast<milliseconds>(Clock::now() - start).count());
}

}

ConvertStats convert(const Config& cfg) {
    const auto start = Clock::now();
    ChainedLines lines(cfg.inputs);
    auto sample = read_sample(lines, INFER_SAMPLE_ROWS);
    auto schema = resolve_schema(cfg, sample);
    if (!schema) {
        auto stats = make_ConvertStats();
        stats.final_state = PipelineState::ABORTED;
        stats.elapsed_ms = elapsed_ms(start);
        return stats;
    }
    auto builder = make_batch_builder(*schema);
    JsonParser parser;
    BatchSink sink(builder, cfg.type_conflict);
    Ctx ctx{cfg, parser, sink, builder, nullptr, make_ConvertStats(), 0};
    auto result = pump(ctx, sample, lines);
    if (result) result = finalize(ctx);
    set_final_state(ctx.stats, result);
    ctx.stats.elapsed_ms = elapsed_ms(start);
    return ctx.stats;
}

std::expected<InferredSchema, std::string> infer_schema(const Config& cfg) {
    ChainedLines lines(cfg.inputs);
    auto sample = read_sample(lines, INFER_SAMPLE_ROWS);
    return resolve_schema(cfg, sample);
}

}
