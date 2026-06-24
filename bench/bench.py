#!/usr/bin/env python3
"""Benchmark Riffle against common JSON-lines -> Parquet one-liners.

Measures wall-clock time and peak resident memory (RSS) by running each tool as
a subprocess and polling its memory while it converts the same dataset. Results
are written to bench/results.json for plotting.

Usage: python bench/bench.py
"""
import json
import os
import statistics
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
OUT = ROOT / "tmp_bench_out.parquet"


def riffle_cmd(src: str) -> list[str]:
    return [str(RIFFLE), src, "-o", str(OUT)]


def py_cmd(code: str, src: str) -> list[str]:
    return [PYTHON, "-c", code.replace("SRC", repr(src)).replace("OUT", repr(str(OUT)))]


TOOLS = {
    "riffle": lambda src: riffle_cmd(src),
    "duckdb": lambda src: py_cmd(
        "import duckdb; duckdb.sql(\"COPY (SELECT * FROM "
        "read_json_auto(SRC, format='newline_delimited')) TO OUT (FORMAT parquet)\")",
        src,
    ),
    "pyarrow": lambda src: py_cmd(
        "import pyarrow.json as j, pyarrow.parquet as p; p.write_table(j.read_json(SRC), OUT)",
        src,
    ),
    "pandas": lambda src: py_cmd(
        "import pandas as pd; pd.read_json(SRC, lines=True).to_parquet(OUT)", src
    ),
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


def bench_tool(name: str, make_cmd, src: Path, size_mb: float) -> dict:
    times, mems = [], []
    for _ in range(REPEATS):
        wall, mem = run_once(make_cmd(str(src)))
        times.append(wall)
        mems.append(mem)
    best = min(times)
    return {
        "tool": name,
        "seconds": round(statistics.median(times), 3),
        "seconds_best": round(best, 3),
        "throughput_mb_s": round(size_mb / best, 1),
        "peak_rss_mb": round(max(mems), 1),
    }


def main() -> None:
    if not RIFFLE.exists():
        sys.exit(f"build the release binary first: {RIFFLE} not found")
    results = []
    for label, src in DATASETS:
        size_mb = src.stat().st_size / (1024 * 1024)
        for name, make_cmd in TOOLS.items():
            row = bench_tool(name, make_cmd, src, size_mb)
            row["dataset"] = label
            results.append(row)
            print(f"{label:18} {name:8} {row['seconds']:6.2f}s "
                  f"{row['throughput_mb_s']:7.1f} MB/s  peak {row['peak_rss_mb']:7.1f} MB")
    (ROOT / "bench" / "results.json").write_text(json.dumps(results, indent=2))
    if OUT.exists():
        OUT.unlink()


if __name__ == "__main__":
    main()
