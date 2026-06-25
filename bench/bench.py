#!/usr/bin/env python3
"""Benchmark Riffle against common JSON-lines -> Parquet one-liners.

Every tool is measured twice for a fair comparison: **single-threaded** (one core
each) and **multi-threaded** (all cores). DuckDB is pinned with `SET threads`,
PyArrow via ReadOptions(use_threads)+set_cpu_count, Riffle via `--threads`.
pandas has no parallel JSON reader, so it is shown as-is in both groups.

Wall-clock time and peak resident memory (RSS) are measured by running each tool
as a subprocess and polling its memory. Results go to bench/results.json.

Usage: python bench/bench.py
"""
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import psutil

ROOT = Path(__file__).resolve().parent.parent
RIFFLE = ROOT / "build-release" / "riffle.exe"
UCRT_BIN = r"C:\msys64\ucrt64\bin"
PYTHON = sys.executable  # native Windows python with duckdb/pyarrow/pandas

DATASETS = [
    ("1M rows (120 MB)", ROOT / "bench" / "data" / "bench.jsonl"),
    ("3M rows (359 MB)", ROOT / "bench" / "data" / "bench3m.jsonl"),
]
REPEATS = 3
THREADS = 8  # "all cores" target for the multi-threaded group
OUT = ROOT / "tmp_bench_out.parquet"


def riffle_cmd(src: str, threads: int) -> list[str]:
    cmd = [str(RIFFLE), src, "-o", str(OUT)]
    return cmd + ["--threads", str(threads)] if threads > 1 else cmd


def py_cmd(code: str, src: str) -> list[str]:
    return [PYTHON, "-c", code.replace("SRC", repr(src)).replace("OUT", repr(str(OUT)))]


def duckdb_code(threads: int) -> str:
    pragma = f"SET threads TO {threads}; " if threads else ""
    return ('import duckdb; duckdb.sql("' + pragma + "COPY (SELECT * FROM "
            "read_json_auto(SRC, format='newline_delimited')) TO OUT (FORMAT parquet)\")")


def pyarrow_code(use_threads: bool) -> str:
    setup = "" if use_threads else "pa.set_cpu_count(1); "
    return ("import pyarrow as pa, pyarrow.json as j, pyarrow.parquet as p; " + setup
            + f"p.write_table(j.read_json(SRC, read_options=j.ReadOptions(use_threads={use_threads})), OUT)")


PANDAS = "import pandas as pd; pd.read_json(SRC, lines=True).to_parquet(OUT)"

MODES = {
    "single": {
        "riffle": lambda s: riffle_cmd(s, 1),
        "duckdb": lambda s: py_cmd(duckdb_code(1), s),
        "pyarrow": lambda s: py_cmd(pyarrow_code(False), s),
        "pandas": lambda s: py_cmd(PANDAS, s),
    },
    "multi": {
        "riffle": lambda s: riffle_cmd(s, THREADS),
        "duckdb": lambda s: py_cmd(duckdb_code(0), s),
        "pyarrow": lambda s: py_cmd(pyarrow_code(True), s),
        "pandas": lambda s: py_cmd(PANDAS, s),
    },
}


def env_with_ucrt() -> dict:
    env = dict(os.environ)
    env["PATH"] = UCRT_BIN + os.pathsep + env.get("PATH", "")
    return env


def run_once(cmd: list[str]) -> tuple[float, float]:
    """Return (wall_seconds, peak_rss_mb) for one subprocess run."""
    start = time.perf_counter()
    proc = psutil.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                        env=env_with_ucrt())
    peak = 0
    while proc.poll() is None:
        try:
            peak = max(peak, proc.memory_info().rss)
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            break
        time.sleep(0.005)
    proc.wait()
    return time.perf_counter() - start, peak / (1024 * 1024)


def bench_tool(make_cmd, src: Path, size_mb: float) -> tuple[float, float]:
    times, mems = [], []
    for _ in range(REPEATS):
        wall, mem = run_once(make_cmd(str(src)))
        times.append(wall)
        mems.append(mem)
    best = min(times)
    return round(size_mb / best, 1), round(max(mems), 1)


def main() -> None:
    if not RIFFLE.exists():
        sys.exit(f"build the release binary first: {RIFFLE} not found")
    results = []
    for label, src in DATASETS:
        size_mb = src.stat().st_size / (1024 * 1024)
        for mode, tools in MODES.items():
            for name, make_cmd in tools.items():
                mbps, peak = bench_tool(make_cmd, src, size_mb)
                results.append({"tool": name, "mode": mode, "dataset": label,
                                "throughput_mb_s": mbps, "peak_rss_mb": peak})
                print(f"{label:18} {mode:6} {name:8} {mbps:7.1f} MB/s  peak {peak:7.1f} MB")
    (ROOT / "bench" / "results.json").write_text(json.dumps(results, indent=2))
    if OUT.exists():
        OUT.unlink()


if __name__ == "__main__":
    main()
