#!/usr/bin/env python3
import argparse
import ast
import csv
import itertools
import json
import statistics
import subprocess
import tempfile
import textwrap
import time
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_EXE = REPO_ROOT / "build" / "crux"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run a Crux benchmark across a grid of GC settings."
    )
    parser.add_argument(
        "benchmark",
        type=Path,
        nargs="?",
        default=Path("tests/benchmarks"),
        help="Path to a benchmark .crux file or a directory of benchmarks. Default: tests/benchmarks",
    )
    parser.add_argument(
        "--exe",
        type=Path,
        default=DEFAULT_EXE,
        help=f"Path to the crux executable. Default: {DEFAULT_EXE}",
    )
    parser.add_argument(
        "--growth",
        type=float,
        nargs="+",
        default=[1.5, 1.75, 2.0, 3.0],
        help="Heap growth factors to test.",
    )
    parser.add_argument(
        "--min-heap",
        type=int,
        nargs="+",
        default=[1024 * 1024 * 0.5, 1024 * 1024, 2 * 1024 * 1024],
        help="Minimum heap sizes to test, in bytes.",
    )
    parser.add_argument(
        "--min-growth",
        type=int,
        nargs="+",
        default=[256 * 1024, 512 * 1024, 1024 * 1024],
        help="Minimum post-GC growth deltas to test, in bytes.",
    )
    parser.add_argument(
        "--repeat",
        type=int,
        default=2,
        help="How many times to run each configuration.",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=10,
        help="How many best configurations to print.",
    )
    parser.add_argument(
        "--sort-by",
        choices=["wall_ms", "gc_total_ns", "collections", "gc_sweep_ns"],
        default="wall_ms",
        help="Primary sort key for the summary output.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Print the full result set as JSON.",
    )
    parser.add_argument(
        "--csv",
        type=Path,
        help="Write the summarized results to a CSV file.",
    )
    return parser.parse_args()


def benchmark_paths(target: Path) -> list[Path]:
    if target.is_file():
        return [target]
    if target.is_dir():
        return sorted(
            path
            for path in target.glob("*.crux")
            if path.is_file() and ".gc_harness." not in path.name
        )
    raise FileNotFoundError(f"Benchmark target not found: {target}")


def benchmark_uses_gc_stats(source: str) -> bool:
    return 'from "crux:gc"' in source and "stats" in source


def benchmark_prints_gc_stats(source: str) -> bool:
    return "println(stats())" in source


def build_injected_source(
    original_source: str, growth: float, min_heap: int, min_growth: int
) -> str:
    needs_stats_import = not benchmark_uses_gc_stats(original_source)
    needs_stats_print = not benchmark_prints_gc_stats(original_source)

    import_names = "set_heap_growth, set_min_heap, set_min_growth"
    if needs_stats_import:
        import_names += ", stats"

    prelude = textwrap.dedent(
        f"""\
        use {import_names} from "crux:gc";
        set_heap_growth({growth});
        set_min_heap({min_heap});
        set_min_growth({min_growth});

        """
    )
    suffix = "\nprintln(stats());\n" if needs_stats_print else ""
    return prelude + original_source + suffix


def extract_stats(stdout: str) -> dict[str, Any]:
    for line in reversed(stdout.splitlines()):
        stripped = line.strip()
        if stripped.startswith("{") and stripped.endswith("}"):
            return ast.literal_eval(stripped)
    raise ValueError("No trailing gc.stats() table found in benchmark output.")


def run_once(
    exe: Path, benchmark: Path, growth: float, min_heap: int, min_growth: int
) -> dict[str, Any]:
    source = benchmark.read_text(encoding="utf-8")
    injected = build_injected_source(source, growth, min_heap, min_growth)

    with tempfile.NamedTemporaryFile(
        "w",
        suffix=".crux",
        prefix=f"{benchmark.stem}.gc_harness.",
        dir=benchmark.parent,
        delete=False,
    ) as handle:
        temp_path = Path(handle.name)
        handle.write(injected)

    started = time.perf_counter()
    try:
        completed = subprocess.run(
            [str(exe), str(temp_path)],
            cwd=REPO_ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
    finally:
        temp_path.unlink(missing_ok=True)
    wall_ms = (time.perf_counter() - started) * 1000.0

    if completed.returncode != 0:
        raise RuntimeError(
            f"Benchmark failed with exit code {completed.returncode}\n"
            f"STDOUT:\n{completed.stdout}\n"
            f"STDERR:\n{completed.stderr}"
        )

    stats = extract_stats(completed.stdout)
    return {
        "benchmark": str(benchmark.relative_to(REPO_ROOT)),
        "growth": growth,
        "min_heap": min_heap,
        "min_growth": min_growth,
        "wall_ms": wall_ms,
        "gc_total_ns": stats.get("total_ns", 0),
        "gc_sweep_ns": stats.get("sweep_ns", 0),
        "collections": stats.get("collections", 0),
        "stats": stats,
    }


def summarize_runs(runs: list[dict[str, Any]]) -> dict[str, Any]:
    first = runs[0]
    wall_samples = [run["wall_ms"] for run in runs]
    gc_total_samples = [run["gc_total_ns"] for run in runs]
    gc_sweep_samples = [run["gc_sweep_ns"] for run in runs]
    collection_samples = [run["collections"] for run in runs]

    return {
        "benchmark": first["benchmark"],
        "growth": first["growth"],
        "min_heap": first["min_heap"],
        "min_growth": first["min_growth"],
        "wall_ms": statistics.mean(wall_samples),
        "wall_ms_min": min(wall_samples),
        "wall_ms_max": max(wall_samples),
        "gc_total_ns": statistics.mean(gc_total_samples),
        "gc_sweep_ns": statistics.mean(gc_sweep_samples),
        "collections": statistics.mean(collection_samples),
        "last_stats": first["stats"] if len(runs) == 1 else runs[-1]["stats"],
    }


def print_summary(results: list[dict[str, Any]], sort_by: str, top: int) -> None:
    ordered = sorted(results, key=lambda item: item[sort_by])
    print(f"Top {min(top, len(ordered))} configs sorted by {sort_by}:")
    for index, result in enumerate(ordered[:top], start=1):
        print(
            f"{index:2d}. benchmark={result['benchmark']} "
            f"growth={result['growth']:<4} "
            f"min_heap={result['min_heap']:<8} "
            f"min_growth={result['min_growth']:<8} "
            f"wall_ms={result['wall_ms']:.2f} "
            f"gc_total_ns={int(result['gc_total_ns'])} "
            f"gc_sweep_ns={int(result['gc_sweep_ns'])} "
            f"collections={result['collections']:.0f}"
        )


def write_csv(path: Path, results: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "benchmark",
        "growth",
        "min_heap",
        "min_growth",
        "wall_ms",
        "wall_ms_min",
        "wall_ms_max",
        "gc_total_ns",
        "gc_sweep_ns",
        "collections",
        "last_next_gc",
        "last_objects_before_sweep",
        "last_objects_after_sweep",
        "last_objects_freed",
        "last_bytes_before",
        "last_bytes_after",
        "last_bytes_freed",
        "last_sweep_slots_scanned",
        "last_pool_capacity",
        "last_live_objects",
        "last_strings_count",
        "last_strings_capacity",
        "last_strings_tombstones",
    ]

    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for result in results:
            stats = result["last_stats"]
            writer.writerow(
                {
                    "benchmark": result["benchmark"],
                    "growth": result["growth"],
                    "min_heap": result["min_heap"],
                    "min_growth": result["min_growth"],
                    "wall_ms": f"{result['wall_ms']:.2f}",
                    "wall_ms_min": f"{result['wall_ms_min']:.2f}",
                    "wall_ms_max": f"{result['wall_ms_max']:.2f}",
                    "gc_total_ns": int(result["gc_total_ns"]),
                    "gc_sweep_ns": int(result["gc_sweep_ns"]),
                    "collections": f"{result['collections']:.0f}",
                    "last_next_gc": stats.get("last_next_gc", 0),
                    "last_objects_before_sweep": stats.get(
                        "last_objects_before_sweep", 0
                    ),
                    "last_objects_after_sweep": stats.get(
                        "last_objects_after_sweep", 0
                    ),
                    "last_objects_freed": stats.get("last_objects_freed", 0),
                    "last_bytes_before": stats.get("last_bytes_before", 0),
                    "last_bytes_after": stats.get("last_bytes_after", 0),
                    "last_bytes_freed": stats.get("last_bytes_freed", 0),
                    "last_sweep_slots_scanned": stats.get(
                        "last_sweep_slots_scanned", 0
                    ),
                    "last_pool_capacity": stats.get("last_pool_capacity", 0),
                    "last_live_objects": stats.get("last_live_objects", 0),
                    "last_strings_count": stats.get("last_strings_count", 0),
                    "last_strings_capacity": stats.get("last_strings_capacity", 0),
                    "last_strings_tombstones": stats.get("last_strings_tombstones", 0),
                }
            )


def main() -> int:
    args = parse_args()
    benchmark_target = (
        (REPO_ROOT / args.benchmark).resolve()
        if not args.benchmark.is_absolute()
        else args.benchmark
    )
    exe = (REPO_ROOT / args.exe).resolve() if not args.exe.is_absolute() else args.exe

    if not exe.exists():
        raise FileNotFoundError(f"Executable not found: {exe}")
    if args.repeat < 1:
        raise ValueError("--repeat must be at least 1")

    benchmarks = benchmark_paths(benchmark_target)
    if not benchmarks:
        raise FileNotFoundError(
            f"No benchmark .crux files found under: {benchmark_target}"
        )

    grouped_results: list[dict[str, Any]] = []

    for benchmark in benchmarks:
        for growth, min_heap, min_growth in itertools.product(
            args.growth, args.min_heap, args.min_growth
        ):
            runs: list[dict[str, Any]] = []
            print(
                f"Running {benchmark.relative_to(REPO_ROOT)} with growth={growth}, "
                f"min_heap={min_heap}, min_growth={min_growth} ({args.repeat} run(s))"
            )
            for _ in range(args.repeat):
                runs.append(run_once(exe, benchmark, growth, min_heap, min_growth))
            grouped_results.append(summarize_runs(runs))

    print_summary(grouped_results, args.sort_by, args.top)

    if args.csv:
        csv_path = (
            (REPO_ROOT / args.csv).resolve() if not args.csv.is_absolute() else args.csv
        )
        write_csv(csv_path, grouped_results)
        print(f"Wrote CSV results to {csv_path}")

    if args.json:
        print(json.dumps(grouped_results, indent=2))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
