#!/usr/bin/env python3
"""Merge comparative-benchmark CSVs into one Markdown report.

Reads every CSV given on the command line (or, with no arguments, every CSV it can find under
the default Tier A output directory and benchmarks/reference/*) and renders one Markdown table
per workload. Tier A (adapter_kind embedded-native/embedded-orm) and Tier B
(cross-language-reference, see benchmarks/reference/README.md) are always rendered as separate
sections so the non-apples-to-apples reference numbers can't be misread as directly comparable
to the in-process C++ tier. See docs/benchmarks.md for the full methodology.

Usage:
    python3 benchmarks/analyze.py [csv_file ...]
"""

from __future__ import annotations

import csv
import glob
import sys
from collections import defaultdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

TIER_A_KINDS = {"embedded-native", "embedded-orm"}
TIER_B_KINDS = {"cross-language-reference"}

COLUMNS = [
    ("engine", "Engine"),
    ("adapter_kind", "Kind"),
    ("durability_profile", "Durability"),
    ("dataset_size", "Dataset"),
    ("iterations", "N"),
    ("mean_us", "Mean (us)"),
    ("p50_us", "p50 (us)"),
    ("p95_us", "p95 (us)"),
    ("p99_us", "p99 (us)"),
    ("throughput_per_s", "Ops/s"),
    ("disk_bytes", "Disk (bytes)"),
]


def default_csv_paths() -> list[Path]:
    paths = []
    tier_a = REPO_ROOT / "opheap-comparative-benchmark-output" / "comparative_summary.csv"
    if tier_a.exists():
        paths.append(tier_a)
    paths.extend(Path(p) for p in glob.glob(str(REPO_ROOT / "benchmarks" / "reference" / "*" / "*.csv")))
    return paths


def load_rows(paths: list[Path]) -> list[dict]:
    rows: list[dict] = []
    for path in paths:
        with path.open(newline="") as f:
            rows.extend(csv.DictReader(f))
    return rows


def format_row(row: dict) -> str:
    cells = []
    for key, _ in COLUMNS:
        value = row.get(key, "")
        if key in ("mean_us", "p50_us", "p95_us", "p99_us"):
            value = f"{float(value):.2f}"
        elif key == "throughput_per_s":
            value = f"{float(value):.1f}"
        cells.append(str(value))
    return "| " + " | ".join(cells) + " |"


def render_table(rows: list[dict]) -> str:
    header = "| " + " | ".join(label for _, label in COLUMNS) + " |"
    separator = "|" + "|".join("---" for _ in COLUMNS) + "|"
    ordered = sorted(rows, key=lambda r: (r["engine"], r["durability_profile"], r["dataset_size"]))
    lines = [header, separator] + [format_row(r) for r in ordered]
    return "\n".join(lines)


def render_report(rows: list[dict]) -> str:
    by_workload: dict[str, list[dict]] = defaultdict(list)
    for row in rows:
        by_workload[row["workload"]].append(row)

    sections = ["# Comparative benchmark report", ""]
    for workload in sorted(by_workload):
        workload_rows = by_workload[workload]
        tier_a = [r for r in workload_rows if r["adapter_kind"] in TIER_A_KINDS]
        tier_b = [r for r in workload_rows if r["adapter_kind"] in TIER_B_KINDS]
        sections.append(f"## {workload}")
        sections.append("")
        if tier_a:
            sections.append("### Tier A — in-process C++ (fair comparison)")
            sections.append("")
            sections.append(render_table(tier_a))
            sections.append("")
        if tier_b:
            sections.append(
                "### Tier B — cross-language reference (separate process/runtime — NOT apples-to-apples)"
            )
            sections.append("")
            sections.append(render_table(tier_b))
            sections.append("")
    return "\n".join(sections)


def main(argv: list[str]) -> int:
    paths = [Path(p) for p in argv] if argv else default_csv_paths()
    if not paths:
        print("no benchmark CSVs found; run opheap_benchmark_comparative first "
              "(see docs/benchmarks.md)", file=sys.stderr)
        return 1
    missing = [p for p in paths if not p.exists()]
    if missing:
        print(f"missing CSV file(s): {', '.join(str(p) for p in missing)}", file=sys.stderr)
        return 1

    rows = load_rows(paths)
    print(render_report(rows))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
