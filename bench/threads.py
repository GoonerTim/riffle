#!/usr/bin/env python3
"""Measure Riffle throughput as the worker-thread count grows and plot it.

Runs the release binary on the benchmark dataset for several --threads values
(best of REPEATS wall-clock times) and writes docs/img/bench_threads.png plus
bench/threads_results.json.

Usage: python bench/threads.py
"""
import json
import os
import subprocess
import time
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent.parent
RIFFLE = ROOT / "build-release" / "riffle.exe"
DATA = ROOT / "bench" / "data" / "bench.jsonl"
OUT = ROOT / "tmp_threads_out.parquet"
UCRT_BIN = r"C:\msys64\ucrt64\bin"
THREAD_COUNTS = [1, 2, 4, 8]
REPEATS = 3


def env_with_ucrt() -> dict:
    env = dict(os.environ)
    env["PATH"] = UCRT_BIN + os.pathsep + env.get("PATH", "")
    return env


def best_seconds(threads: int) -> float:
    best = float("inf")
    for _ in range(REPEATS):
        start = time.perf_counter()
        subprocess.run([str(RIFFLE), str(DATA), "-o", str(OUT), "--threads", str(threads)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       env=env_with_ucrt())
        best = min(best, time.perf_counter() - start)
    return best


def main() -> None:
    size_mb = DATA.stat().st_size / (1024 * 1024)
    rows = []
    for n in THREAD_COUNTS:
        secs = best_seconds(n)
        mbps = round(size_mb / secs, 1)
        rows.append({"threads": n, "seconds": round(secs, 3), "throughput_mb_s": mbps})
        print(f"threads={n}: {secs:.3f}s  {mbps} MB/s")
    (ROOT / "bench" / "threads_results.json").write_text(json.dumps(rows, indent=2))

    fig, ax = plt.subplots(figsize=(8, 4.5))
    xs = [r["threads"] for r in rows]
    ys = [r["throughput_mb_s"] for r in rows]
    ax.plot(xs, ys, marker="o", color="#2a9d8f", linewidth=2)
    for x, y in zip(xs, ys):
        ax.text(x, y, f"{y:.0f}", ha="center", va="bottom", fontsize=9)
    ax.set_xticks(xs)
    ax.set_xlabel("Worker threads (--threads)")
    ax.set_ylabel("MB/s of input")
    ax.set_title("Riffle throughput scales with --threads (120 MB dataset)")
    ax.grid(alpha=0.3)
    ax.set_ylim(bottom=0)
    fig.tight_layout()
    (ROOT / "docs" / "img").mkdir(parents=True, exist_ok=True)
    fig.savefig(ROOT / "docs" / "img" / "bench_threads.png", dpi=120)
    print("wrote", ROOT / "docs" / "img" / "bench_threads.png")
    if OUT.exists():
        OUT.unlink()


if __name__ == "__main__":
    main()
