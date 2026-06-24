# Riffle

**🇬🇧 English** · [🇷🇺 Русский](README.ru.md)

> Streaming **JSON-lines → Parquet** converter for huge logs — built in C++, fast, and
> memory-bounded.

`riffle` turns terabyte-scale `.jsonl` files into analytics-ready **Apache Parquet** on a
single machine, at disk speed, **without loading the input into RAM**. It fills the gap
between "grep the log by hand" and "spin up Spark/pandas/DuckDB for one conversion".

The name is a double metaphor: a *riffle* is both a quick shuffle of a card deck
(restructuring rows into columns) and water running over stones (streaming).

---

## Why Riffle

- **Throughput first** — SIMD JSON parsing ([simdjson](https://github.com/simdjson/simdjson))
  and columnar batching target on the order of **≥ 1 GB/s** on a single core.
- **Constant memory** — one-pass streaming with bounded batches; RAM usage is **O(1)** in the
  size of the input, not the size of the file.
- **Zero-config** — the column schema is **inferred** from the data, with an optional explicit
  `--schema` override when inference guesses wrong.
- **One static binary** — no Python/JVM runtime. Drops cleanly into pipelines and containers.

See the full design in [`docs/riffle.md`](docs/riffle.md).

## Status

⚠️ **Early stage.** The specification ([`docs/riffle.md`](docs/riffle.md)) is complete;
implementation is in progress. APIs and flags may change.

## Quick start

### Install

```bash
# Debian/Ubuntu system packages
apt-get update && apt-get install -y \
    build-essential cmake git \
    libarrow-dev libparquet-dev \
    libzstd-dev libsnappy-dev

# simdjson (header-only) as a submodule
git clone https://github.com/simdjson/simdjson third_party/simdjson

# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Use

```bash
# File -> Parquet (schema inferred automatically)
riffle events.jsonl -o events.parquet

# Stream from stdin, zstd compression, 100k-row batches
gunzip -c huge.jsonl.gz | riffle - -o out.parquet --compression zstd --batch-rows 100000

# Multiple files via glob + explicit schema override
riffle 'logs/*.jsonl' -o merged.parquet --schema schema.json

# Keep going on bad lines, collect them, print stats to stderr
riffle events.jsonl -o out.parquet --on-error collect --stats
```

### Library

```cpp
#include <riffle.hpp>

int main() {
    riffle::Config cfg = riffle::make_Config(
        /*inputs=*/{"events.jsonl"},
        /*output_path=*/"events.parquet");
    riffle::ConvertStats stats = riffle::convert(cfg);
    return stats.final_state == riffle::PipelineState::DONE ? 0 : 1;
}
```

## CLI flags

| Flag                | Argument                       | Default   | Effect                                  |
| ------------------- | ------------------------------ | --------- | --------------------------------------- |
| `-o, --output`      | path                           | required  | Output file                             |
| `--format`          | `parquet` \| `columnar-raw`    | `parquet` | Output format                           |
| `--schema`          | path to JSON                   | none      | Explicit schema, overrides inference    |
| `--compression`     | `none` \| `snappy` \| `zstd`   | `zstd`    | Parquet codec                           |
| `--batch-rows`      | integer                        | `65536`   | Rows per batch                          |
| `--on-error`        | `skip` \| `abort` \| `collect` | `skip`    | Policy for malformed lines              |
| `--type-conflict`   | `widen` \| `string` \| `error` | `widen`   | Column type-conflict resolution         |
| `--stats`           | —                              | off       | Print conversion stats to stderr        |
| `-h, --help`        | —                              | —         | Show usage                              |

## Development

This repo uses [`just`](https://github.com/casey/just) as a task runner:

```bash
just            # list tasks
just build      # configure + build (Release)
just test       # run the test suite
just fmt        # format sources (clang-format)
just lint       # static analysis (clang-tidy)
```

CI runs build, tests, format and lint checks on every push and pull request via GitHub
Actions (see [`.github/workflows/ci.yml`](.github/workflows/ci.yml)).

## Contributing

Contributions are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md)
([на русском](CONTRIBUTING.ru.md)).

## License

[MIT](LICENSE) © 2026 Riffle contributors.
