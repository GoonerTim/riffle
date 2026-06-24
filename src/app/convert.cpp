#include "riffle/convert.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "riffle/batch.hpp"
#include "riffle/factories.hpp"
#include "riffle/ports.hpp"
#include "riffle/reader.hpp"
#include "riffle/schema.hpp"
#include "riffle/simdjson_source.hpp"
#include "riffle/writer.hpp"

namespace riffle {
namespace {

using Clock = std::chrono::steady_clock;

std::unique_ptr<std::istream> open_stream(const std::string& path) {
    if (path == "-") return std::make_unique<std::istream>(std::cin.rdbuf());
    return std::make_unique<std::ifstream>(path);
}

// RowSource over an in-memory sample (used to drive schema inference).
class VectorSource : public RowSource {
public:
    explicit VectorSource(const std::vector<Row>& rows) : rows_(rows) {}
    std::optional<Row> next() override {
        if (index_ >= rows_.size()) return std::nullopt;
        return rows_[index_++];
    }

private:
    const std::vector<Row>& rows_;
    std::size_t index_ = 0;
};

// RowSource that reads each configured input in turn, accumulating parse errors.
class ChainedSource : public RowSource {
public:
    explicit ChainedSource(std::vector<std::string> paths) : paths_(std::move(paths)) { advance(); }

    std::optional<Row> next() override {
        while (source_) {
            if (auto row = source_->next()) return row;
            absorb_errors();
            advance();
        }
        return std::nullopt;
    }

    const std::vector<ParseError>& errors() const { return errors_; }

private:
    void absorb_errors() {
        if (!source_) return;
        for (const auto& error : source_->errors()) errors_.push_back(error);
    }

    void advance() {
        source_.reset();
        reader_.reset();
        stream_.reset();
        if (index_ >= paths_.size()) return;
        stream_ = open_stream(paths_[index_++]);
        reader_ = std::make_unique<LineReader>(*stream_);
        source_ = std::make_unique<SimdjsonSource>(*reader_);
    }

    std::vector<std::string> paths_;
    std::size_t index_ = 0;
    std::unique_ptr<std::istream> stream_;
    std::unique_ptr<LineReader> reader_;
    std::unique_ptr<SimdjsonSource> source_;
    std::vector<ParseError> errors_;
};

struct PipelineCtx {
    const Config& cfg;
    std::unique_ptr<Writer> writer;  // opened lazily on first flush
    BatchBuilder builder;
    ConvertStats stats;
};

// Reconstruct the (possibly widened) schema currently held by the builder.
InferredSchema current_schema(const BatchBuilder& builder) {
    InferredSchema schema;
    for (const auto& column : builder.columns) schema.columns.push_back(column.schema);
    return schema;
}

std::vector<Row> read_sample(RowSource& source, std::size_t limit) {
    std::vector<Row> sample;
    for (auto row = source.next(); row && sample.size() < limit; row = source.next()) {
        sample.push_back(std::move(*row));
    }
    return sample;
}

InferredSchema build_schema_for(const Config& cfg, const std::vector<Row>& sample) {
    VectorSource source(sample);
    auto schema = infer_schema(source, cfg.type_conflict);
    if (cfg.schema_override.columns.empty()) return schema;
    return merge_override(schema, cfg.schema_override);
}

std::expected<void, std::string> ensure_writer(PipelineCtx& ctx) {
    if (ctx.writer) return {};
    auto writer = open_writer(ctx.cfg, current_schema(ctx.builder));
    if (!writer) return std::unexpected(writer.error());
    ctx.writer = std::move(*writer);
    return {};
}

std::expected<void, std::string> flush(PipelineCtx& ctx) {
    if (ctx.builder.n_rows == 0) return {};
    if (auto ok = ensure_writer(ctx); !ok) return ok;
    auto batch = build_batch(ctx.builder);
    if (!batch) return std::unexpected(batch.error());
    ctx.stats.rows_written += batch->n_rows;
    return ctx.writer->write(*batch);
}

std::expected<void, std::string> append_one(PipelineCtx& ctx, const Row& row) {
    if (auto ok = append_row(ctx.builder, row, ctx.cfg.type_conflict); !ok) return ok;
    ++ctx.stats.rows_read;
    if (ctx.builder.n_rows >= ctx.cfg.batch_rows) return flush(ctx);
    return {};
}

std::expected<void, std::string> pump(PipelineCtx& ctx, std::vector<Row>& sample, RowSource& src) {
    for (auto& row : sample) {
        if (auto ok = append_one(ctx, row); !ok) return ok;
    }
    for (auto row = src.next(); row; row = src.next()) {
        if (auto ok = append_one(ctx, *row); !ok) return ok;
    }
    return flush(ctx);
}

void apply_error_policy(ConvertStats& stats, const Config& cfg,
                        const std::vector<ParseError>& errors) {
    stats.rows_skipped = errors.size();
    if (cfg.on_error == OnError::COLLECT) stats.errors = errors;
    if (cfg.on_error == OnError::ABORT && !errors.empty()) stats.final_state = PipelineState::ABORTED;
}

void set_final_state(ConvertStats& stats, const std::expected<void, std::string>& result) {
    if (!result || stats.final_state == PipelineState::ABORTED) {
        stats.final_state = PipelineState::ABORTED;
        return;
    }
    stats.final_state = PipelineState::DONE;
}

std::expected<void, std::string> finalize(PipelineCtx& ctx) {
    if (auto ok = ensure_writer(ctx); !ok) return ok;  // empty input → valid empty file
    return ctx.writer->finish();
}

ConvertStats run_pipeline(PipelineCtx ctx, std::vector<Row>& sample, ChainedSource& source) {
    auto result = pump(ctx, sample, source);
    if (result) result = finalize(ctx);
    apply_error_policy(ctx.stats, ctx.cfg, source.errors());
    set_final_state(ctx.stats, result);
    return ctx.stats;
}

std::uint64_t elapsed_ms(Clock::time_point start) {
    using std::chrono::duration_cast, std::chrono::milliseconds;
    return static_cast<std::uint64_t>(duration_cast<milliseconds>(Clock::now() - start).count());
}

}  // namespace

ConvertStats convert(const Config& cfg) {
    const auto start = Clock::now();
    ChainedSource source(cfg.inputs);
    auto sample = read_sample(source, INFER_SAMPLE_ROWS);
    auto schema = build_schema_for(cfg, sample);
    PipelineCtx ctx{cfg, nullptr, make_batch_builder(schema), make_ConvertStats()};
    auto stats = run_pipeline(std::move(ctx), sample, source);
    stats.elapsed_ms = elapsed_ms(start);
    return stats;
}

}  // namespace riffle
