#!/usr/bin/env python3
"""Render benchmark charts from bench/results.json into docs/img/.

Produces a single-threaded and a multi-threaded comparison for each of
throughput and peak memory (four charts), so every tool is compared like for
like.

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
    data, datasets = {}, []
    for row in rows:
        datasets.append(row["dataset"])
        data[(row["mode"], row["dataset"], row["tool"])] = row
    return list(dict.fromkeys(datasets)), data


def grouped_bar(datasets, data, mode, metric, title, ylabel, fname):
    fig, ax = plt.subplots(figsize=(8, 4.5))
    width = 0.2
    xs = range(len(datasets))
    for i, tool in enumerate(TOOLS):
        vals = [data[(mode, d, tool)][metric] for d in datasets]
        offs = [x + (i - 1.5) * width for x in xs]
        bars = ax.bar(offs, vals, width, label=tool, color=COLORS[tool])
        for b, v in zip(bars, vals):
            ax.text(b.get_x() + b.get_width() / 2, v, f"{v:.0f}", ha="center",
                    va="bottom", fontsize=8)
    ax.set_xticks(list(xs))
    ax.set_xticklabels(datasets)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend(fontsize=8)
    ax.grid(axis="y", alpha=0.3)
    ax.set_ylim(bottom=0)
    fig.tight_layout()
    IMG.mkdir(parents=True, exist_ok=True)
    fig.savefig(IMG / fname, dpi=120)
    print("wrote", IMG / fname)


def main():
    datasets, data = load()
    for mode, tag in (("single", "1 core each"), ("multi", "all cores")):
        grouped_bar(datasets, data, mode, "throughput_mb_s",
                    f"Throughput — {tag} (higher is better)", "MB/s of input",
                    f"bench_throughput_{mode}.png")
        grouped_bar(datasets, data, mode, "peak_rss_mb",
                    f"Peak memory — {tag} (lower is better)", "Peak RSS, MB",
                    f"bench_memory_{mode}.png")


if __name__ == "__main__":
    main()
