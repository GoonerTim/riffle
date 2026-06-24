#!/usr/bin/env python3
"""Generate a deterministic JSON-lines dataset for benchmarking.

Usage: python gen_data.py <rows> <output.jsonl>
Each record mixes a timestamp, strings, an int and a double — the shapes Riffle
infers and writes as a typed Parquet schema.
"""
import json
import sys


LEVELS = ["info", "warning", "error", "debug"]
PATHS = ["/api/v1/users", "/api/v1/orders", "/health", "/metrics", "/login"]


def main() -> None:
    rows = int(sys.argv[1])
    out_path = sys.argv[2]
    with open(out_path, "w", encoding="utf-8", newline="\n") as out:
        for i in range(rows):
            sec = i % 86400
            record = {
                "ts": f"2026-06-24T{sec // 3600:02d}:{sec % 3600 // 60:02d}:{sec % 60:02d}Z",
                "level": LEVELS[i % len(LEVELS)],
                "path": PATHS[i % len(PATHS)],
                "status": 200 + (i % 5) * 100,
                "latency_ms": round((i % 1000) * 0.37, 3),
                "user_id": i % 100000,
            }
            out.write(json.dumps(record))
            out.write("\n")


if __name__ == "__main__":
    main()
