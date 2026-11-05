#!/usr/bin/env python3
"""Times opening files with two import workers, alternating runs, and prints the medians.

    compare-allocators.py --benchmark loupe-import-benchmark \
        --worker mimalloc=loupe-worker --worker system=loupe-worker-system-allocator \
        --runs 7 stage.step grid.step

Each run is a fresh worker process opening the file, as the app does. Runs alternate between
the workers so a noisy runner affects both alike. The worker's own report of the allocator in
effect is printed beside its times, so a mimalloc build whose override did not take shows up.
"""
import argparse
import json
import statistics
import subprocess
import sys


def run(benchmark, worker, path):
    output = subprocess.run([benchmark, "--worker", worker, path], capture_output=True, text=True, timeout=600)
    lines = [line for line in output.stdout.splitlines() if line.startswith("{")]
    if output.returncode != 0 or not lines:
        raise SystemExit(f"benchmark failed for {worker} on {path}: {output.stderr.strip()[-400:]}")
    return json.loads(lines[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--benchmark", required=True)
    parser.add_argument("--worker", action="append", required=True, help="label=path")
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("files", nargs="+")
    arguments = parser.parse_args()
    workers = [entry.split("=", 1) for entry in arguments.worker]

    keys = ("treeReadyMs", "previewReadyMs", "finalReadyMs")
    print(f"{'file':28} {'worker':10} {'allocator':34} " + " ".join(f"{key[:-2]:>12}" for key in keys))
    medians = {}
    for path in arguments.files:
        results = {label: [] for label, _ in workers}
        for _ in range(arguments.runs):
            for label, worker in workers:
                results[label].append(run(arguments.benchmark, worker, path))
        for label, _ in workers:
            runs = results[label]
            values = {key: statistics.median(run[key] for run in runs) for key in keys}
            medians[(path, label)] = values
            allocator = runs[-1].get("allocator", "?")
            name = path.replace("\\", "/").rsplit("/", 1)[-1]
            print(f"{name:28} {label:10} {allocator:34} " + " ".join(f"{values[key]:>12.0f}" for key in keys))
    if len(workers) == 2:
        (first, _), (second, _) = workers
        for path in arguments.files:
            a = medians[(path, first)]["finalReadyMs"]
            b = medians[(path, second)]["finalReadyMs"]
            name = path.replace("\\", "/").rsplit("/", 1)[-1]
            print(f"{name}: {first} {a:.0f} ms vs {second} {b:.0f} ms to ready ({(a - b) / b * 100:+.1f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
