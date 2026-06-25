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


def color_for(tool: str) -> str:
    if tool.startswith("riffle"):
        return "#9ad1c9" if "(1t)" in tool else "#2a9d8f"
    return {"duckdb": "#e9c46a", "pyarrow": "#f4a261", "pandas": "#e76f51"}.get(tool, "#888888")


def load():
    rows = json.loads(RESULTS.read_text())
    datasets, tools, data = [], [], {}
    for row in rows:
        datasets.append(row["dataset"])
        tools.append(row["tool"])
        data.setdefault(row["dataset"], {})[row["tool"]] = row
    return list(dict.fromkeys(datasets)), list(dict.fromkeys(tools)), data


def grouped_bar(datasets, tools, data, metric, title, ylabel, fname, annotate):
    fig, ax = plt.subplots(figsize=(9, 4.5))
    width = 0.8 / len(tools)
    xs = range(len(datasets))
    for i, tool in enumerate(tools):
        vals = [data[d][tool][metric] for d in datasets]
        offs = [x + (i - (len(tools) - 1) / 2) * width for x in xs]
        bars = ax.bar(offs, vals, width, label=tool, color=color_for(tool))
        for b, v in zip(bars, vals):
            ax.text(b.get_x() + b.get_width() / 2, v, annotate(v), ha="center",
                    va="bottom", fontsize=7)
    ax.set_xticks(list(xs))
    ax.set_xticklabels(datasets)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend(fontsize=8)
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    IMG.mkdir(parents=True, exist_ok=True)
    fig.savefig(IMG / fname, dpi=120)
    print("wrote", IMG / fname)


def main():
    datasets, tools, data = load()
    grouped_bar(datasets, tools, data, "peak_rss_mb",
                "Peak memory (lower is better) — Riffle 1t stays flat with input size",
                "Peak RSS, MB", "bench_memory.png", lambda v: f"{v:.0f}")
    grouped_bar(datasets, tools, data, "throughput_mb_s",
                "Throughput (higher is better)",
                "MB/s of input", "bench_throughput.png", lambda v: f"{v:.0f}")


if __name__ == "__main__":
    main()
