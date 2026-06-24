# Riffle

**🇬🇧 English** · [🇷🇺 Русский](README.ru.md)

> Streaming **JSON-lines → Parquet** converter for huge logs — built in C++, fast, and
> memory-bounded.

`riffle` turns terabyte-scale `.jsonl` files into analytics-ready **Apache Parquet** on a
single machine, at disk speed, **without loading the input into RAM**. It fills the gap
between "grep the log by hand" and "spin up Spark/pandas/DuckDB for one conversion".

---

## Why Riffle

- **Constant memory** — one-pass streaming with bounded batches; RAM usage is **O(1)** in the
  size of the input, not the size of the file. This is the headline feature: convert files
  **larger than RAM**.
- **Fast single-core** — SIMD JSON parsing ([simdjson](https://github.com/simdjson/simdjson))
  on-demand and batched columnar appends sustain **~115 MB/s** of input on one core.
- **Zero-config** — the column schema is **inferred** from the data, with an optional explicit
  `--schema` override when inference guesses wrong.
- **One static binary** — no Python/JVM runtime. Drops cleanly into pipelines and containers.

See the full design in [`docs/riffle.md`](docs/riffle.md).

## Who it's for

- **Data / backend engineers** who need JSON logs in a columnar store for analytics, without
  standing up Spark or a Python data stack.
- **DevOps / SRE** turning application, access or audit logs (`.jsonl`) into Parquet to query
  with DuckDB, Spark, Trino/Athena, or pandas.
- **Anyone on a laptop or small VM** who must convert a file **larger than available RAM** —
  Riffle's flat memory footprint is exactly for this.
- **Pipeline / container authors** who prefer a single dependency-free binary over a
  Python/JVM runtime in an ETL step.

## Use cases

- **Log archival & analytics** — convert NDJSON application/access/audit logs to compact
  Parquet for cheap columnar storage and fast queries.
- **ETL ingestion stage** — a fast, memory-bounded "JSON-lines → Parquet" step inside a larger
  pipeline (cron job, Airflow task, container step).
- **Data-lake landing** — normalize heterogeneous JSON events into typed Parquet with an
  inferred schema (or a pinned `--schema`).
- **Ad-hoc conversion** — turn a huge one-off `.jsonl` dump into Parquet on a laptop where
  pandas/pyarrow would run out of memory.
- **Streaming from pipes** — `gunzip -c big.jsonl.gz | riffle - -o out.parquet`, converting on
  the fly without a temporary file.
- **As a library** — embed `riffle::convert` in a C++ service to emit Parquet from JSON without
  pulling in a heavy data framework.

## Benchmarks

Converting the same JSON-lines dataset to Parquet, Riffle vs. the common Python one-liners
(`duckdb`, `pyarrow.json`, `pandas`). Measured on the development machine; reproduce with
`just bench` (each tool run as a subprocess 3×, best wall-time, peak RSS polled via psutil).

### Peak memory — the reason Riffle exists

![Peak memory comparison](docs/img/bench_memory.png)

Riffle streams in **constant memory**: its peak stays at **~80 MB whether the input is 120 MB
or 359 MB**. The others load the whole file (or large intermediates): pandas peaks at **4.2 GB
on a 359 MB input (~12×)**, pyarrow at ~810 MB, duckdb at ~490 MB and growing with input size.
That flat line is why Riffle converts files **larger than RAM** on a laptop where the others OOM.

### Throughput — honest picture

![Throughput comparison](docs/img/bench_throughput.png)

Riffle is **not** the throughput leader. DuckDB (~355–595 MB/s) and PyArrow (~260–395 MB/s) are
faster; Riffle sustains **~115 MB/s**, ahead of pandas (~35 MB/s). Parsing uses the simdjson
**on-demand** API with `string_view`-backed cells (no per-field allocation), fields are written
**straight into column builders**, and Arrow appends are **batched** per column. The remaining
cost is Arrow array construction and string materialization; compression codec barely moves it.

### Scaling with `--threads`

![Throughput vs threads](docs/img/bench_threads.png)

Single-threaded throughput is mid-pack, but conversion parallelizes across cores: with
`--threads 8` Riffle reaches **~380 MB/s** on the 120 MB dataset — into DuckDB/PyArrow territory —
while keeping the same flat, bounded memory and **byte-identical, deterministic output**
(workers parse+build chunks; a single writer emits batches in input order).

### Where Riffle wins / where it doesn't

| Criterion                          | Riffle                | duckdb | pyarrow | pandas |
| ---------------------------------- | --------------------- | ------ | ------- | ------ |
| Peak memory, flat with input size  | ✅ ~80 MB, constant    | ⚠️ grows | ❌ grows | ❌ huge |
| Converts files larger than RAM     | ✅                     | ⚠️      | ❌       | ❌      |
| Raw throughput                     | ⚠️ ~115 MB/s           | ✅      | ✅       | ❌      |
| Single static binary, no runtime   | ✅                     | ❌ (lib) | ❌ (lib) | ❌ (lib) |

**Bottom line:** if you need raw speed on data that fits in memory, DuckDB is excellent. If you
need to convert **arbitrarily large logs in bounded memory** from a single dependency-free
binary, that is exactly what Riffle is for.

## Status

🚧 **Working MVP.** JSON-lines → Parquet (and `columnar-raw`) conversion works end-to-end
(library + CLI), built test-first with 100+ tests. C++23. Schema is inferred (including ISO-8601
timestamps); nested objects are flattened; column types auto-widen beyond the inference sample;
`--schema` JSON override, column projection (`--select`/`--exclude`/`--rename`), transparent
gzip/zstd input, and multi-threaded conversion (`--threads`, deterministic output) are supported.
Known limitations: with `--threads > 1` the schema is fixed from the inference sample (no
cross-batch widening); nested data is flattened to dotted columns, not native Parquet structs.

## Quick start

### Install

```bash
# Debian/Ubuntu: Arrow/Parquet come from the Apache Arrow APT repository,
# not the stock Ubuntu repos.
sudo apt-get update
sudo apt-get install -y -V ca-certificates lsb-release wget
wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt-get install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt-get update

# Build dependencies
sudo apt-get install -y -V \
    build-essential cmake ninja-build \
    libarrow-dev libparquet-dev libzstd-dev libsnappy-dev \
    libgtest-dev

# Build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Use

```bash
# File -> Parquet (schema inferred automatically)
riffle events.jsonl -o events.parquet

# Gzip/zstd input is decompressed transparently (by extension)
riffle huge.jsonl.gz -o out.parquet

# Parallel conversion across cores (deterministic output, bounded memory)
riffle huge.jsonl -o out.parquet --threads 8

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
| `--batch-bytes`     | integer                        | 256 MiB   | Byte cap per batch                      |
| `--threads`         | integer                        | `1`       | Parallel worker threads                 |
| `--on-error`        | `skip` \| `abort` \| `collect` | `skip`    | Policy for malformed lines              |
| `--type-conflict`   | `widen` \| `string` \| `error` | `widen`   | Column type-conflict resolution         |
| `--select`          | `col,col,...`                  | all       | Keep only these columns (in this order) |
| `--exclude`         | `col,col,...`                  | none      | Drop these columns                      |
| `--rename`          | `from=to,...`                  | none      | Rename output columns                   |
| `--print-schema`    | —                              | off       | Print the inferred schema as JSON, then exit |
| `--stats`           | —                              | off       | Print conversion stats to stderr        |
| `-h, --help`        | —                              | —         | Show help and exit                      |
| `--version`         | —                              | —         | Show version and exit                   |

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
