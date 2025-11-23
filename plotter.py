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


def prepare_series(data: List[Dict]) -> Dict[str, List[Tuple[int, float, float, float, float]]]:
    """Group and sort data by workload_type.

    Returns a dict mapping workload_type -> list of
    (threads, throughput, avg_response_ms, avg_cpu_percent, avg_disk_write_kbps)
    sorted by threads.
    """
    # Use a dict for each workload to store items by thread count.
    # This ensures that if we encounter duplicates for the same thread count,
    # the previous one is overwritten.
    grouped_by_threads: Dict[str, Dict[int, Tuple[int, float, float, float, float]]] = defaultdict(dict)
    for row in data:
        wt = row.get("workload_type")
        if wt is None:
            continue
        threads = int(row.get("threads", 0))
        throughput = float(row.get("throughput", 0.0))
        avg_ms = float(row.get("avg_response_ms", 0.0))
        cpu = float(row.get("avg_cpu_percent", 0.0))
        disk_write = float(row.get("avg_disk_write_kbps", 0.0))
        grouped_by_threads[wt][threads] = (threads, throughput, avg_ms, cpu, disk_write)

    # Convert back to a list of tuples and sort by threads
    grouped: Dict[str, List[Tuple[int, float, float, float, float]]] = {}
    for wt, items_dict in grouped_by_threads.items():
        grouped[wt] = sorted(items_dict.values(), key=lambda x: x[0])

    return grouped


def plot_throughput(series: Dict[str, List[Tuple[int, float, float]]], outpath: str, dpi: int = 150):
    plt.figure(figsize=(8, 5), dpi=dpi)
    for wt, items in series.items():
        threads = [t for t, _, _, _, _ in items]
        throughput = [tp for _, tp, _, _, _ in items]
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
        threads = [t for t, _, _, _, _ in items]
        avg_ms = [ms for _, _, ms, _, _ in items]
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


def plot_for_workload(wt: str, items: List[Tuple[int, float, float, float, float]], outdir: str, dpi: int = 150):
    """Create two plots for a single workload type: throughput and response time.

    Saves files as:
      throughput_<wt>.png and response_time_<wt>.png
    """
    safe = _sanitize_name(wt)
    threads = [t for t, _, _, _, _ in items]
    throughput = [tp for _, tp, _, _, _ in items]
    avg_ms = [ms for _, _, ms, _, _ in items]

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


def plot_combined_for_workload(
        wt: str, items: List[Tuple[int, float, float, float, float]], outdir: str, dpi: int = 150
) -> str:
    """Create a combined 2x2 plot for a single workload type.

    Subplots:
        - Threads vs Throughput
        - Threads vs Avg response time (ms)
        - Threads vs Avg CPU percent
        - Threads vs Avg disk write (KB/s)

    Returns path to saved combined image.
    """
    safe = _sanitize_name(wt)
    threads = [t for t, _, _, _, _ in items]
    throughput = [tp for _, tp, _, _, _ in items]
    avg_ms = [ms for _, _, ms, _, _ in items]
    cpu = [c for _, _, _, c, _ in items]
    disk_write = [d for _, _, _, _, d in items]

    fig, axs = plt.subplots(2, 2, figsize=(12, 8), dpi=dpi)

    axs[0, 0].plot(threads, throughput, marker="o")
    axs[0, 0].set_xlabel("Threads")
    axs[0, 0].set_ylabel("Throughput (requests/sec)")
    axs[0, 0].set_title("Throughput")
    axs[0, 0].grid(True, linestyle="--", alpha=0.4)

    axs[0, 1].plot(threads, avg_ms, marker="o", color="tab:orange")
    axs[0, 1].set_xlabel("Threads")
    axs[0, 1].set_ylabel("Avg response time (ms)")
    axs[0, 1].set_title("Avg Response Time")
    axs[0, 1].grid(True, linestyle="--", alpha=0.4)

    axs[1, 0].plot(threads, cpu, marker="o", color="tab:green")
    axs[1, 0].set_xlabel("Threads")
    axs[1, 0].set_ylabel("Avg CPU %")
    axs[1, 0].set_title("CPU Usage")
    axs[1, 0].grid(True, linestyle="--", alpha=0.4)

    axs[1, 1].plot(threads, disk_write, marker="o", color="tab:red")
    axs[1, 1].set_xlabel("Threads")
    axs[1, 1].set_ylabel("Avg disk write (KB/s)")
    axs[1, 1].set_title("Disk Write")
    axs[1, 1].grid(True, linestyle="--", alpha=0.4)

    fig.suptitle(f"Performance — {wt}")
    fig.tight_layout(rect=[0, 0.03, 1, 0.95])
    outpath = os.path.join(outdir, f"combined_{safe}.png")
    fig.savefig(outpath)
    plt.close(fig)
    return outpath


def plot_three_for_workload(
    wt: str, items: List[Tuple[int, float, float, float, float]], outdir: str, dpi: int = 150
) -> str:
    """Create a horizontal 1x3 plot for a workload: throughput, response time, CPU %.

    Returns path to saved image.
    """
    safe = _sanitize_name(wt)
    threads = [t for t, _, _, _, _ in items]
    throughput = [tp for _, tp, _, _, _ in items]
    avg_ms = [ms for _, _, ms, _, _ in items]
    cpu = [c for _, _, _, c, _ in items]

    fig, axs = plt.subplots(1, 3, figsize=(15, 4), dpi=dpi)

    axs[0].plot(threads, throughput, marker="o")
    axs[0].set_xlabel("Threads")
    axs[0].set_ylabel("Throughput (requests/sec)")
    axs[0].set_title("Throughput")
    axs[0].grid(True, linestyle="--", alpha=0.4)

    axs[1].plot(threads, avg_ms, marker="o", color="tab:orange")
    axs[1].set_xlabel("Threads")
    axs[1].set_ylabel("Avg response time (ms)")
    axs[1].set_title("Avg Response Time")
    axs[1].grid(True, linestyle="--", alpha=0.4)

    axs[2].plot(threads, cpu, marker="o", color="tab:green")
    axs[2].set_xlabel("Threads")
    axs[2].set_ylabel("Avg CPU %")
    axs[2].set_title("CPU Usage")
    axs[2].grid(True, linestyle="--", alpha=0.4)

    fig.suptitle(f"Performance — {wt}")
    fig.tight_layout(rect=[0, 0.03, 1, 0.95])
    outpath = os.path.join(outdir, f"combined_three_{safe}.png")
    fig.savefig(outpath)
    plt.close(fig)
    return outpath


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
        # combined 2x2 figure (throughput, response time, cpu, disk write)
        combined_path = plot_combined_for_workload(wt, items, args.outdir, dpi=args.dpi)
        created.append(combined_path)
        # for "popular" workloads also create a 1x3 combined plot (throughput, response time, cpu)
        if "popular" in wt.lower():
            combined3_path = plot_three_for_workload(wt, items, args.outdir, dpi=args.dpi)
            created.append(combined3_path)

    for p in created:
        print(f"Saved: {p}")

    if args.show:
        # re-create figures for interactive viewing
        for wt, items in series.items():
            threads = [t for t, _, _, _, _ in items]
            throughput = [tp for _, tp, _, _, _ in items]
            avg_ms = [ms for _, _, ms, _, _ in items]

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
