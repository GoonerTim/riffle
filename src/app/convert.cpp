#include "riffle/convert.hpp"

#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "riffle/batch.hpp"
#include "riffle/factories.hpp"
#include "riffle/input.hpp"
#include "riffle/json_parser.hpp"
#include "riffle/nested.hpp"
#include "riffle/reader.hpp"
#include "riffle/schema.hpp"
#include "riffle/writer.hpp"
#include "riffle/writer_backends.hpp"

namespace riffle {
namespace {

using Clock = std::chrono::steady_clock;

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
        source_.reset();
        if (index_ >= paths_.size()) return;
        source_ = open_input(paths_[index_++]);
        reader_ = std::make_unique<LineReader>(*source_);
    }

    std::vector<std::string> paths_;
    std::size_t index_ = 0;
    std::unique_ptr<ByteSource> source_;
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
    if (ctx.builder.n_rows >= ctx.cfg.batch_rows || ctx.builder.byte_size >= ctx.cfg.batch_bytes)
        return flush(ctx);
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

ConvertStats run_single(const Config& cfg, const InferredSchema& schema,
                        const std::vector<std::string>& sample, ChainedLines& lines) {
    auto builder = make_batch_builder(schema);
    JsonParser parser;
    BatchSink sink(builder, cfg.type_conflict);
    Ctx ctx{cfg, parser, sink, builder, nullptr, make_ConvertStats(), 0};
    auto result = pump(ctx, sample, lines);
    if (result) result = finalize(ctx);
    set_final_state(ctx.stats, result);
    return ctx.stats;
}

struct NestedCtx {
    const Config& cfg;
    NestedBuilder& builder;
    Writer& writer;
    ConvertStats stats;
    std::size_t line_no = 0;
};

std::expected<void, std::string> nested_flush(NestedCtx& ctx) {
    if (ctx.builder.rows() == 0) return {};
    auto batch = ctx.builder.flush();
    if (!batch) return std::unexpected(batch.error());
    ctx.stats.rows_written += batch->n_rows;
    return ctx.writer.write(*batch);
}

std::expected<void, std::string> nested_process(NestedCtx& ctx, std::string_view line) {
    ++ctx.line_no;
    if (auto ok = ctx.builder.append_line(line); !ok) {
        if (ctx.cfg.on_error == OnError::ABORT) return std::unexpected(ok.error());
        ++ctx.stats.rows_skipped;
        if (ctx.cfg.on_error == OnError::COLLECT)
            ctx.stats.errors.push_back(make_ParseError(
                {.line_no = ctx.line_no, .reason = ok.error(), .raw = std::string(line)}));
        return {};
    }
    ++ctx.stats.rows_read;
    if (ctx.builder.rows() >= ctx.cfg.batch_rows) return nested_flush(ctx);
    return {};
}

std::expected<void, std::string> nested_pump(NestedCtx& ctx, const std::vector<std::string>& sample,
                                             ChainedLines& lines) {
    for (const auto& line : sample)
        if (auto ok = nested_process(ctx, line); !ok) return ok;
    for (auto line = lines.next(); line; line = lines.next())
        if (auto ok = nested_process(ctx, *line); !ok) return ok;
    return nested_flush(ctx);
}

ConvertStats aborted_stats() {
    auto stats = make_ConvertStats();
    stats.final_state = PipelineState::ABORTED;
    return stats;
}

ConvertStats run_nested(const Config& cfg, const std::vector<std::string>& sample,
                        ChainedLines& lines) {
    auto top = infer_nested_schema(sample);
    if (!top) return aborted_stats();
    auto builder = NestedBuilder::make(*top);
    if (!builder) return aborted_stats();
    auto writer = open_parquet_writer_arrow(cfg, nested_arrow_schema(*top));
    if (!writer) return aborted_stats();
    auto built = std::move(*builder);
    auto sink = std::move(*writer);
    NestedCtx ctx{cfg, *built, *sink, make_ConvertStats(), 0};
    auto result = nested_pump(ctx, sample, lines);
    if (result) result = sink->finish();
    set_final_state(ctx.stats, result);
    return ctx.stats;
}

struct Chunk {
    std::size_t seq = 0;
    std::size_t first_line = 0;
    std::vector<std::string> lines;
};

// Workers fill native column buffers only; all Arrow array construction happens
// later on the single writer thread, since Arrow builders/pool are not safe to
// drive concurrently across threads.
struct ChunkResult {
    BatchBuilder builder;
    std::vector<ParseError> errors;
    std::size_t rows_read = 0;
    std::size_t rows_skipped = 0;
    bool fatal = false;
};

ChunkResult process_chunk(const Config& cfg, const InferredSchema& schema, const Chunk& chunk) {
    JsonParser parser;
    auto builder = make_batch_builder(schema);
    BatchSink sink(builder, cfg.type_conflict, /*allow_widen=*/false);
    ChunkResult res;
    std::size_t line_no = chunk.first_line;
    for (const auto& line : chunk.lines) {
        ++line_no;
        auto ok = parser.parse(line, sink);
        if (ok) {
            ++res.rows_read;
            continue;
        }
        if (sink.fatal() || cfg.on_error == OnError::ABORT) {
            res.fatal = true;
            break;
        }
        ++res.rows_skipped;
        if (cfg.on_error == OnError::COLLECT) {
            res.errors.push_back(
                make_ParseError({.line_no = line_no, .reason = ok.error(), .raw = line}));
        }
    }
    res.builder = std::move(builder);
    return res;
}

// Parse+build chunks on worker threads; a single writer drains results in
// sequence order so the output is deterministic and memory stays bounded.
// All shared state lives under one mutex with one condition variable, so every
// predicate reads consistent state and no wakeup can be lost.
class ParallelExecutor {
public:
    ParallelExecutor(const Config& cfg, const InferredSchema& schema)
        : cfg_(cfg), schema_(schema), max_in_flight_(2 * cfg.threads + 2) {}

    ConvertStats run(const std::vector<std::string>& sample, ChainedLines& lines) {
        std::vector<std::thread> workers;
        workers.reserve(cfg_.threads);
        for (std::size_t i = 0; i < cfg_.threads; ++i)
            workers.emplace_back([this] { worker_loop(); });
        std::thread producer([&] { produce(sample, lines); });
        write_loop();
        producer.join();
        for (auto& worker : workers) worker.join();
        return stats_;
    }

private:
    bool push_chunk(std::size_t seq, std::size_t first_line, std::vector<std::string>&& lines) {
        std::unique_lock lock(mtx_);
        cv_.wait(lock, [&] { return in_flight_ < max_in_flight_ || aborted_; });
        if (aborted_) return false;
        in_queue_.push(Chunk{seq, first_line, std::move(lines)});
        ++in_flight_;
        lock.unlock();
        cv_.notify_all();
        return true;
    }

    void produce(const std::vector<std::string>& sample, ChainedLines& lines) {
        std::size_t produced = 0;
        std::size_t line_no = 0;
        std::vector<std::string> buf;
        auto feed = [&](std::string&& line) {
            buf.push_back(std::move(line));
            ++line_no;
            if (buf.size() < cfg_.batch_rows) return true;
            auto moved = std::move(buf);
            buf.clear();
            return push_chunk(produced++, line_no - moved.size(), std::move(moved));
        };
        bool ok = true;
        for (const auto& line : sample)
            if (!feed(std::string(line))) {
                ok = false;
                break;
            }
        if (ok)
            for (auto line = lines.next(); line; line = lines.next())
                if (!feed(std::string(*line))) break;
        if (ok && !buf.empty()) push_chunk(produced++, line_no - buf.size(), std::move(buf));
        std::unique_lock lock(mtx_);
        total_ = produced;
        input_done_ = true;
        lock.unlock();
        cv_.notify_all();
    }

    void worker_loop() {
        while (true) {
            Chunk chunk;
            bool aborted = false;
            {
                std::unique_lock lock(mtx_);
                cv_.wait(lock, [&] { return !in_queue_.empty() || input_done_; });
                if (in_queue_.empty()) return;
                chunk = std::move(in_queue_.front());
                in_queue_.pop();
                aborted = aborted_;
            }
            ChunkResult res = aborted ? ChunkResult{} : process_chunk(cfg_, schema_, chunk);
            {
                std::lock_guard lock(mtx_);
                results_.emplace(chunk.seq, std::move(res));
            }
            cv_.notify_all();
        }
    }

    std::expected<void, std::string> ensure_writer() {
        if (writer_) return {};
        auto writer = open_writer(cfg_, schema_);
        if (!writer) return std::unexpected(writer.error());
        writer_ = std::move(*writer);
        return {};
    }

    void abort() {
        std::lock_guard lock(mtx_);
        aborted_ = true;
    }

    void consume(ChunkResult& res, std::expected<void, std::string>& result) {
        if (!result) return;
        if (res.fatal) {
            result = std::unexpected("aborted");
            abort();
            return;
        }
        stats_.rows_read += res.rows_read;
        stats_.rows_skipped += res.rows_skipped;
        for (auto& error : res.errors) stats_.errors.push_back(std::move(error));
        if (res.builder.n_rows == 0) return;
        if (auto ok = ensure_writer(); !ok) {
            result = ok;
            abort();
            return;
        }
        auto batch = build_batch(res.builder);
        if (!batch) {
            result = std::unexpected(batch.error());
            abort();
            return;
        }
        stats_.rows_written += batch->n_rows;
        if (auto ok = writer_->write(*batch); !ok) {
            result = ok;
            abort();
        }
    }

    bool take_result(std::size_t expected, ChunkResult& out) {
        std::unique_lock lock(mtx_);
        cv_.wait(lock, [&] {
            return results_.contains(expected) || (input_done_ && expected >= total_);
        });
        if (!results_.contains(expected)) return false;
        out = std::move(results_[expected]);
        results_.erase(expected);
        --in_flight_;
        lock.unlock();
        cv_.notify_all();
        return true;
    }

    void write_loop() {
        std::expected<void, std::string> result;
        ChunkResult res;
        for (std::size_t expected = 0; take_result(expected, res); ++expected) consume(res, result);
        if (result) {
            if (auto ok = ensure_writer(); ok)
                result = writer_->finish();
            else
                result = ok;
        }
        set_final_state(stats_, result);
    }

    const Config& cfg_;
    const InferredSchema& schema_;
    const std::size_t max_in_flight_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<Chunk> in_queue_;
    std::map<std::size_t, ChunkResult> results_;
    std::size_t in_flight_ = 0;
    std::size_t total_ = 0;
    bool input_done_ = false;
    bool aborted_ = false;
    std::unique_ptr<Writer> writer_;
    ConvertStats stats_ = make_ConvertStats();
};

ConvertStats run_parallel(const Config& cfg, const InferredSchema& schema,
                          const std::vector<std::string>& sample, ChainedLines& lines) {
    ParallelExecutor executor(cfg, schema);
    return executor.run(sample, lines);
}

}

ConvertStats convert(const Config& cfg) {
    const auto start = Clock::now();
    ChainedLines lines(cfg.inputs);
    auto sample = read_sample(lines, INFER_SAMPLE_ROWS);
    if (cfg.nested == NestedMode::NATIVE) {
        auto stats = run_nested(cfg, sample, lines);
        stats.elapsed_ms = elapsed_ms(start);
        return stats;
    }
    auto schema = resolve_schema(cfg, sample);
    if (!schema) {
        auto stats = aborted_stats();
        stats.elapsed_ms = elapsed_ms(start);
        return stats;
    }
    auto stats = cfg.threads <= 1 ? run_single(cfg, *schema, sample, lines)
                                  : run_parallel(cfg, *schema, sample, lines);
    stats.elapsed_ms = elapsed_ms(start);
    return stats;
}

std::expected<InferredSchema, std::string> infer_schema(const Config& cfg) {
    ChainedLines lines(cfg.inputs);
    auto sample = read_sample(lines, INFER_SAMPLE_ROWS);
    return resolve_schema(cfg, sample);
}

}
