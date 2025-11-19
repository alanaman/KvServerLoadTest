#!/usr/bin/env python3
"""Simple plotter for load test results.

Reads a JSON results file (default: build/results.json) containing a list of
measurements with fields: threads, workload_type, throughput, avg_response_ms
and produces two PNG plots saved into the requested output directory:

- throughput_vs_threads.png
- response_time_vs_threads.png

Usage:
    python plotter.py --input build/results.json --outdir build/plots --show

"""
from __future__ import annotations

import argparse
import json
import os
from collections import defaultdict
from typing import Dict, List, Tuple

import matplotlib.pyplot as plt


def load_results(path: str) -> List[Dict]:
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, list):
        raise ValueError(f"expected a list in {path}")
    return data


def prepare_series(data: List[Dict]) -> Dict[str, List[Tuple[int, float, float]]]:
    """Group and sort data by workload_type.

    Returns a dict mapping workload_type -> list of (threads, throughput, avg_response_ms)
    sorted by threads.
    """
    grouped: Dict[str, List[Tuple[int, float, float]]] = defaultdict(list)
    for row in data:
        wt = row.get("workload_type")
        if wt is None:
            continue
        threads = int(row.get("threads", 0))
        throughput = float(row.get("throughput", 0.0))
        avg_ms = float(row.get("avg_response_ms", 0.0))
        grouped[wt].append((threads, throughput, avg_ms))

    # sort each series by threads
    for wt, items in grouped.items():
        items.sort(key=lambda x: x[0])
        grouped[wt] = items

    return grouped


def plot_throughput(series: Dict[str, List[Tuple[int, float, float]]], outpath: str, dpi: int = 150):
    plt.figure(figsize=(8, 5), dpi=dpi)
    for wt, items in series.items():
        threads = [t for t, _, _ in items]
        throughput = [tp for _, tp, _ in items]
        plt.plot(threads, throughput, marker="o", label=wt)

    plt.xlabel("Threads")
    plt.ylabel("Throughput (requests/sec)")
    plt.title("Throughput vs Threads")
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.legend()
    plt.tight_layout()
    plt.savefig(outpath)
    plt.close()


def plot_response_time(series: Dict[str, List[Tuple[int, float, float]]], outpath: str, dpi: int = 150):
    plt.figure(figsize=(8, 5), dpi=dpi)
    for wt, items in series.items():
        threads = [t for t, _, _ in items]
        avg_ms = [ms for _, _, ms in items]
        plt.plot(threads, avg_ms, marker="o", label=wt)

    plt.xlabel("Threads")
    plt.ylabel("Avg response time (ms)")
    plt.title("Average Response Time vs Threads")
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.legend()
    plt.tight_layout()
    plt.savefig(outpath)
    plt.close()


def _sanitize_name(name: str) -> str:
    # safe filename fragment
    return "".join(c if c.isalnum() or c in ("-", "_") else "_" for c in name)


def plot_for_workload(wt: str, items: List[Tuple[int, float, float]], outdir: str, dpi: int = 150):
    """Create two plots for a single workload type: throughput and response time.

    Saves files as:
      throughput_<wt>.png and response_time_<wt>.png
    """
    safe = _sanitize_name(wt)
    threads = [t for t, _, _ in items]
    throughput = [tp for _, tp, _ in items]
    avg_ms = [ms for _, _, ms in items]

    # Throughput plot
    plt.figure(figsize=(8, 5), dpi=dpi)
    plt.plot(threads, throughput, marker="o")
    plt.xlabel("Threads")
    plt.ylabel("Throughput (requests/sec)")
    plt.title(f"Throughput vs Threads — {wt}")
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.tight_layout()
    tp_path = os.path.join(outdir, f"throughput_{safe}.png")
    plt.savefig(tp_path)
    plt.close()

    # Response time plot
    plt.figure(figsize=(8, 5), dpi=dpi)
    plt.plot(threads, avg_ms, marker="o")
    plt.xlabel("Threads")
    plt.ylabel("Avg response time (ms)")
    plt.title(f"Avg Response Time vs Threads — {wt}")
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.tight_layout()
    rt_path = os.path.join(outdir, f"response_time_{safe}.png")
    plt.savefig(rt_path)
    plt.close()

    return tp_path, rt_path


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot load test results")
    parser.add_argument("--input", "-i", default="build/results.json", help="path to results.json")
    parser.add_argument("--outdir", "-o", default="build/plots", help="directory to save plots")
    parser.add_argument("--show", action="store_true", help="display plots interactively")
    parser.add_argument("--dpi", type=int, default=150, help="dpi for saved images")
    args = parser.parse_args()

    if not os.path.exists(args.input):
        raise SystemExit(f"input file not found: {args.input}")

    os.makedirs(args.outdir, exist_ok=True)

    data = load_results(args.input)
    series = prepare_series(data)

    if not series:
        raise SystemExit("no data series found in input")

    created = []
    for wt, items in series.items():
        tp_path, rt_path = plot_for_workload(wt, items, args.outdir, dpi=args.dpi)
        created.append(tp_path)
        created.append(rt_path)

    for p in created:
        print(f"Saved: {p}")

    if args.show:
        # re-create figures for interactive viewing
        for wt, items in series.items():
            threads = [t for t, _, _ in items]
            throughput = [tp for _, tp, _ in items]
            avg_ms = [ms for _, _, ms in items]

            plt.figure(figsize=(8, 5))
            plt.plot(threads, throughput, marker="o")
            plt.xlabel("Threads")
            plt.ylabel("Throughput (requests/sec)")
            plt.title(f"Throughput vs Threads — {wt}")
            plt.grid(True, linestyle="--", alpha=0.4)

            plt.figure(figsize=(8, 5))
            plt.plot(threads, avg_ms, marker="o")
            plt.xlabel("Threads")
            plt.ylabel("Avg response time (ms)")
            plt.title(f"Avg Response Time vs Threads — {wt}")
            plt.grid(True, linestyle="--", alpha=0.4)

        plt.show()


if __name__ == "__main__":
    main()
