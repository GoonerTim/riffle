#!/usr/bin/env python3
"""Render benchmark charts from bench/results.json into docs/img/.

Usage: python bench/plot.py
"""
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent.parent
RESULTS = ROOT / "bench" / "results.json"
IMG = ROOT / "docs" / "img"

TOOLS = ["riffle", "duckdb", "pyarrow", "pandas"]
COLORS = {"riffle": "#2a9d8f", "duckdb": "#e9c46a", "pyarrow": "#f4a261", "pandas": "#e76f51"}


def load():
    rows = json.loads(RESULTS.read_text())
    datasets, data = [], {}
    for row in rows:
        datasets.append(row["dataset"])
        data.setdefault(row["dataset"], {})[row["tool"]] = row
    seen = list(dict.fromkeys(datasets))
    return seen, data


def grouped_bar(datasets, data, metric, title, ylabel, fname, annotate):
    fig, ax = plt.subplots(figsize=(8, 4.5))
    width = 0.2
    xs = range(len(datasets))
    for i, tool in enumerate(TOOLS):
        vals = [data[d][tool][metric] for d in datasets]
        offs = [x + (i - 1.5) * width for x in xs]
        bars = ax.bar(offs, vals, width, label=tool, color=COLORS[tool])
        for b, v in zip(bars, vals):
            ax.text(b.get_x() + b.get_width() / 2, v, annotate(v), ha="center",
                    va="bottom", fontsize=8)
    ax.set_xticks(list(xs))
    ax.set_xticklabels(datasets)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    IMG.mkdir(parents=True, exist_ok=True)
    fig.savefig(IMG / fname, dpi=120)
    print("wrote", IMG / fname)


def main():
    datasets, data = load()
    grouped_bar(datasets, data, "peak_rss_mb",
                "Peak memory (lower is better) — Riffle stays flat",
                "Peak RSS, MB", "bench_memory.png", lambda v: f"{v:.0f}")
    grouped_bar(datasets, data, "throughput_mb_s",
                "Throughput (higher is better)",
                "MB/s of input", "bench_throughput.png", lambda v: f"{v:.0f}")


if __name__ == "__main__":
    main()
