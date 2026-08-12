#!/usr/bin/env python3
"""Render docs/benchmarks-infographic.md from a comparative_summary.csv + environment.json.

Regenerable by design (see docs/benchmarks.md's "reproducible benchmark matrix" goal) — numbers
drift with hardware and engine versions, so this reads the CSV instead of hand-transcribing
figures into the page. Palette and mark specs follow the project's dataviz skill: validated
5-slot categorical palette (blue/orange/aqua/yellow/magenta), bars capped at 24-40px, rounded
top / square baseline, direct value labels (required here as the light-mode contrast relief for
the aqua/yellow/magenta slots), a single shared legend, and a linked data table for full values.

Usage:
    python3 benchmarks/render_infographic.py \\
        [opheap-comparative-benchmark-output/comparative_summary.csv] \\
        [opheap-comparative-benchmark-output/environment.json] \\
        [docs/benchmarks-infographic.md]
"""

from __future__ import annotations

import csv
import json
import math
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

ENGINES = ["opheap", "sqlite", "sqlite_orm", "lmdb", "rocksdb"]
ENGINE_LABELS = {"opheap": "opheap", "sqlite": "SQLite", "sqlite_orm": "sqlite_orm", "lmdb": "LMDB",
                  "rocksdb": "RocksDB"}
# Reference-palette categorical slots 1-5 (blue/orange/aqua/yellow/magenta), validated for
# adjacent-bar CVD separation in both light and dark mode — see the dataviz skill's palette.md.
COLORS_LIGHT = {"opheap": "#2a78d6", "sqlite": "#eb6834", "sqlite_orm": "#1baf7a", "lmdb": "#eda100",
                 "rocksdb": "#e87ba4"}
COLORS_DARK = {"opheap": "#3987e5", "sqlite": "#d95926", "sqlite_orm": "#199e70", "lmdb": "#c98500",
                "rocksdb": "#d55181"}


def load_rows(csv_path: Path) -> list[dict]:
    with csv_path.open(newline="") as f:
        return list(csv.DictReader(f))


def find(rows: list[dict], engine: str, workload: str, profile: str, size: str) -> dict | None:
    for r in rows:
        if (r["engine"] == engine and r["workload"] == workload and r["durability_profile"] == profile
                and r["dataset_size"] == size):
            return r
    return None


def fmt_us(v: float) -> str:
    if v >= 1_000_000:
        return f"{v / 1_000_000:.2f} s"
    if v >= 1_000:
        return f"{v / 1_000:.2f} ms"
    return f"{v:.1f} µs"


def fmt_rate(v: float) -> str:
    if v >= 1_000_000:
        return f"{v / 1_000_000:.2f}M/s"
    if v >= 1_000:
        return f"{v / 1_000:.1f}K/s"
    return f"{v:.0f}/s"


def fmt_bytes(v: float) -> str:
    for unit in ("B", "KB", "MB"):
        if v < 1024:
            return f"{v:.0f} {unit}"
        v /= 1024
    return f"{v:.1f} GB"


def esc(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def rounded_top_bar(x: float, y: float, w: float, h: float, r: float = 4.0) -> str:
    r = max(0.0, min(r, w / 2, h))
    if h <= 0:
        return ""
    return (f"M{x},{y + h} L{x},{y + r} Q{x},{y} {x + r},{y} "
            f"L{x + w - r},{y} Q{x + w},{y} {x + w},{y + r} L{x + w},{y + h} Z")


def nice_log_ticks(lo: float, hi: float) -> list[float]:
    start = math.floor(math.log10(max(lo, 1e-9)))
    end = math.ceil(math.log10(hi))
    return [10.0 ** p for p in range(start, end + 1)]


# Bar thickness capped at 24px per the dataviz skill's mark spec (never fill the slot); the
# generous GAP is what keeps five- and six-character engine-name labels legible beneath narrow bars.
BAR_W, GAP, GROUP_GAP = 24.0, 26.0, 46.0
PLOT_H = 170.0
TOP_PAD, LABEL_PAD, CAT_PAD, GROUP_PAD = 26.0, 8.0, 20.0, 18.0
AXIS_LABEL_W = 46.0


def svg_chart(chart_id: str, title: str, groups: list[tuple[str, dict]], value_fmt, log_scale: bool,
              aria_label: str) -> str:
    """groups: [(group_label_or_'', {engine: value}), ...]. One bar per engine per group."""
    all_vals = [v for _, vals in groups for v in vals.values() if v > 0]
    max_val = max(all_vals)
    min_val = min(all_vals)
    has_group_labels = any(g for g, _ in groups)
    height = TOP_PAD + PLOT_H + LABEL_PAD + CAT_PAD + (GROUP_PAD if has_group_labels else 0) + 10

    def scale(v: float) -> float:
        if v <= 0:
            return 0.0
        if log_scale:
            lo, hi = math.log10(max(min_val, 1e-9)), math.log10(max_val)
            if hi == lo:
                hi = lo + 1
            return max(2.0, (math.log10(v) - lo) / (hi - lo) * PLOT_H)
        return (v / max_val) * PLOT_H if max_val else 0.0

    baseline_y = TOP_PAD + PLOT_H
    x = AXIS_LABEL_W
    bars, cat_labels, group_labels = [], [], []
    for group_label, values in groups:
        group_start_x = x
        for engine in ENGINES:
            if engine not in values:
                continue
            v = values[engine]
            h = scale(v)
            y = baseline_y - h
            path = rounded_top_bar(x, y, BAR_W, h)
            label = esc(value_fmt(v))
            bars.append(
                f'<path class="bar bar-{engine}" d="{path}"><title>{esc(ENGINE_LABELS[engine])} '
                f"— {label}</title></path>"
            )
            label_y = max(y - 6, 10.0)  # keep the label on canvas; never push it down into the bar
            bars.append(f'<text class="bar-value" x="{x + BAR_W / 2:.1f}" y="{label_y:.1f}">{label}</text>')
            cat_labels.append(
                f'<text class="bar-cat" x="{x + BAR_W / 2:.1f}" y="{baseline_y + CAT_PAD:.1f}">'
                f'{esc(ENGINE_LABELS[engine])}</text>'
            )
            x += BAR_W + GAP
        if has_group_labels:
            group_center = (group_start_x + x - GAP) / 2
            group_labels.append(
                f'<text class="group-label" x="{group_center:.1f}" '
                f'y="{baseline_y + CAT_PAD + GROUP_PAD:.1f}">{esc(group_label)}</text>'
            )
        x += GROUP_GAP - GAP
    width = x - GROUP_GAP + GAP + AXIS_LABEL_W / 2

    # Gridlines: 4 evenly spaced steps in the active scale, with a value tick label.
    grid = []
    steps = 4
    for i in range(steps + 1):
        if log_scale:
            lo, hi = math.log10(max(min_val, 1e-9)), math.log10(max_val)
            gv = 10 ** (lo + (hi - lo) * i / steps)
        else:
            gv = max_val * i / steps
        gy = baseline_y - scale(gv)
        grid.append(f'<line class="grid" x1="{AXIS_LABEL_W - 6}" y1="{gy:.1f}" x2="{width - 10:.1f}" y2="{gy:.1f}"/>')
        grid.append(f'<text class="axis-tick" x="{AXIS_LABEL_W - 10}" y="{gy + 3:.1f}" text-anchor="end">'
                     f'{esc(value_fmt(gv))}</text>')

    chart_class = "chart chart-wide" if len(groups) > 1 else "chart"
    return f'''<figure class="chart-panel">
  <figcaption><h3>{esc(title)}</h3></figcaption>
  <svg class="{chart_class}" viewBox="0 0 {width:.1f} {height:.1f}" role="img" aria-label="{esc(aria_label)}">
    {''.join(grid)}
    <line class="baseline" x1="{AXIS_LABEL_W - 6}" y1="{baseline_y}" x2="{width - 10:.1f}" y2="{baseline_y}"/>
    {''.join(bars)}
    {''.join(cat_labels)}
    {''.join(group_labels)}
  </svg>
</figure>'''


def legend_html() -> str:
    items = "".join(
        f'<span class="legend-item"><span class="legend-swatch swatch-{e}"></span>{esc(ENGINE_LABELS[e])}</span>'
        for e in ENGINES
    )
    return f'<div class="legend">{items}</div>'


def stat_tiles(rows: list[dict]) -> str:
    vals = {e: float(find(rows, e, "recovery", "durable", "large")["disk_bytes"]) for e in ENGINES}
    ordered = sorted(ENGINES, key=lambda e: vals[e])
    tiles = "".join(
        f'''<div class="stat-tile">
      <span class="stat-accent swatch-{e}"></span>
      <div class="stat-label">{esc(ENGINE_LABELS[e])}</div>
      <div class="stat-value">{esc(fmt_bytes(vals[e]))}</div>
    </div>'''
        for e in ordered
    )
    return f'<div class="stat-row">{tiles}</div>'


def build_page(rows: list[dict], env: dict) -> str:
    durable_small_txn = ("", {e: float(find(rows, e, "small_durable_txn", "durable", "n/a")["mean_us"])
                               for e in ENGINES})
    relaxed_small_txn = ("", {e: float(find(rows, e, "small_durable_txn", "relaxed", "n/a")["mean_us"])
                               for e in ENGINES})
    bulk_load_large = ("", {e: float(find(rows, e, "bulk_load", "durable", "large")["throughput_per_s"])
                             for e in ENGINES})
    point_read_large = ("", {e: float(find(rows, e, "point_read", "durable", "large")["mean_us"])
                              for e in ENGINES})
    range_scan_groups = [
        (size.capitalize(), {e: float(find(rows, e, "range_scan", "durable", size)["mean_us"]) for e in ENGINES})
        for size in ("small", "medium", "large")
    ]

    panel_durability = f'''<div class="chart-pair">
    {svg_chart("durable", "Durable (fsync every commit)", [durable_small_txn], fmt_us, False,
               "Single-record durable transaction latency by engine, microseconds, linear scale")}
    {svg_chart("relaxed", "Relaxed (buffered)", [relaxed_small_txn], fmt_us, False,
               "Single-record relaxed transaction latency by engine, microseconds, linear scale")}
  </div>'''

    panels = "\n".join([
        f'<section class="panel"><h2>Durability cost: one record, one commit</h2>'
        f'<p class="panel-note">Same operation, two profiles. Durable commits cross a real fsync '
        f'barrier; relaxed ones don’t — see the <a href="benchmarks.md">durability-profile mapping</a> '
        f'for what each engine does under the hood.</p>{panel_durability}</section>',

        f'<section class="panel"><h2>Bulk load throughput</h2>'
        f'<p class="panel-note">20,000 rows, one commit, durable profile. Log scale — the gap between '
        f'the embedded KV/SQL engines and opheap’s per-root transaction model is real, not a rounding '
        f'artifact.</p>'
        f'{svg_chart("bulk", "Rows/second, large dataset (log scale)", [bulk_load_large], fmt_rate, True, "Bulk load throughput by engine, rows per second, log scale")}'
        f'</section>',

        f'<section class="panel"><h2>Point read latency</h2>'
        f'<p class="panel-note">300 sampled reads across a 20,000-row dataset, durable profile, log scale.</p>'
        f'{svg_chart("read", "Microseconds per read, large dataset (log scale)", [point_read_large], fmt_us, True, "Point read latency by engine, microseconds, log scale")}'
        f'</section>',

        f'<section class="panel"><h2>Range scan cost grows with dataset size</h2>'
        f'<p class="panel-note">Equality filter + order + limit, no secondary index on any engine — '
        f'the lowest-common-denominator query every adapter can express (see '
        f'<a href="benchmarks.md">methodology</a>). Log scale: opheap’s '
        f'per-root transaction model has no index to lean on, so its cost scales with row count far '
        f'faster than the others’.</p>'
        f'{svg_chart("scan", "Microseconds per scan (log scale)", range_scan_groups, fmt_us, True, "Range scan latency by engine and dataset size, microseconds, log scale")}'
        f'</section>',

        f'<section class="panel"><h2>Disk footprint (20,000-row dataset)</h2>'
        f'<p class="panel-note">Bytes on disk after a checkpoint, durable profile.</p>'
        f'{stat_tiles(rows)}</section>',
    ])

    env_line = (f'{env.get("cpu_model", "unknown CPU")} · {env.get("hardware_threads", "?")} threads · '
                f'{env.get("kernel", "unknown kernel")} · {env.get("compiler", "unknown compiler")} · '
                f'SQLite {env.get("sqlite_version", "?")} · LMDB {env.get("lmdb_version", "?")} · '
                f'RocksDB {env.get("rocksdb_version", "?")}')

    return f'''---
layout: default
title: Benchmarks infographic
description: opheap vs. SQLite, sqlite_orm, LMDB and RocksDB — comparative benchmark results at a glance.
---

<h1>opheap vs. SQLite / sqlite_orm / LMDB / RocksDB</h1>

<p class="panel-note">Tier A of the <a href="benchmarks.md">comparative benchmark suite</a> —
same in-process C++ workloads, same durability profiles, same machine. Not shown here:
opheap wins on write-path simplicity (no schema, no ORM layer) and loses on anything that
benefits from a secondary index, which it doesn’t have yet (<a href="roadmap.md">roadmap
0.3</a>). Full per-workload data: <a href="assets/comparative_summary.csv">raw CSV</a>
&middot; regenerate with <code>python3 benchmarks/analyze.py</code> for a plain-text table.</p>

<p class="env-line">{esc(env_line)}</p>

{legend_html()}

{panels}

<p class="panel-note">Cross-language reference numbers (SQLAlchemy, Diesel) exist too, but aren’t
charted here — they run in a different process/runtime and aren’t apples-to-apples with the
in-process C++ tier above. See <a href="benchmarks.md">Tier B</a>.</p>
'''


INFOGRAPHIC_CSS = '''
<style>
.infographic-root { color-scheme: light dark; }
.legend { display: flex; flex-wrap: wrap; gap: 1.1rem; margin: 1.25rem 0 2rem; font-size: .92rem; }
.legend-item { display: inline-flex; align-items: center; gap: .45rem; color: color-mix(in srgb, currentColor 78%, transparent); }
.legend-swatch, .stat-accent { display: inline-block; width: 12px; height: 12px; border-radius: 3px; }
.env-line { font-size: .85rem; color: color-mix(in srgb, currentColor 55%, transparent); margin-top: -.5rem; }
.panel { margin: 2.5rem 0; }
.panel-note { font-size: .95rem; color: color-mix(in srgb, currentColor 68%, transparent); max-width: 68ch; }
.chart-pair { display: flex; flex-wrap: wrap; gap: 2rem; }
.chart-panel { margin: 0; max-width: 100%; }
.chart-panel figcaption h3 { margin: 0 0 .5rem; font-size: 1rem; font-weight: 600; }
.chart { width: 100%; max-width: 560px; height: auto; display: block; }
.chart-wide { max-width: 100%; }
.chart-pair .chart { max-width: 380px; }
.bar { transition: opacity .12s ease; }
.bar:hover, .bar:focus { opacity: .82; }
.bar-value { font-size: 10.5px; text-anchor: middle; fill: color-mix(in srgb, currentColor 88%, transparent); font-variant-numeric: tabular-nums; }
.bar-cat { font-size: 10.5px; text-anchor: middle; fill: color-mix(in srgb, currentColor 62%, transparent); }
.group-label { font-size: 10.5px; text-anchor: middle; fill: color-mix(in srgb, currentColor 62%, transparent); font-weight: 600; }
.axis-tick { font-size: 9.5px; fill: color-mix(in srgb, currentColor 50%, transparent); font-variant-numeric: tabular-nums; }
.grid { stroke: color-mix(in srgb, currentColor 12%, transparent); stroke-width: 1; }
.baseline { stroke: color-mix(in srgb, currentColor 30%, transparent); stroke-width: 1; }
.stat-row { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px,1fr)); gap: .9rem; margin-top: 1rem; }
.stat-tile { border: 1px solid color-mix(in srgb, currentColor 14%, transparent); border-radius: 12px; padding: .9rem 1rem; position: relative; overflow: hidden; }
.stat-accent { position: absolute; top: 0; left: 0; width: 100%; height: 3px; border-radius: 0; }
.stat-label { font-size: .82rem; color: color-mix(in srgb, currentColor 62%, transparent); margin-top: .35rem; }
.stat-value { font-size: 1.3rem; font-weight: 700; margin-top: .15rem; font-variant-numeric: tabular-nums; }
.swatch-opheap, .bar-opheap { fill: #2a78d6; background: #2a78d6; }
.swatch-sqlite, .bar-sqlite { fill: #eb6834; background: #eb6834; }
.swatch-sqlite_orm, .bar-sqlite_orm { fill: #1baf7a; background: #1baf7a; }
.swatch-lmdb, .bar-lmdb { fill: #eda100; background: #eda100; }
.swatch-rocksdb, .bar-rocksdb { fill: #e87ba4; background: #e87ba4; }
@media (prefers-color-scheme: dark) {
  .swatch-opheap, .bar-opheap { fill: #3987e5; background: #3987e5; }
  .swatch-sqlite, .bar-sqlite { fill: #d95926; background: #d95926; }
  .swatch-sqlite_orm, .bar-sqlite_orm { fill: #199e70; background: #199e70; }
  .swatch-lmdb, .bar-lmdb { fill: #c98500; background: #c98500; }
  .swatch-rocksdb, .bar-rocksdb { fill: #d55181; background: #d55181; }
}
</style>
'''


def main() -> int:
    csv_path = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO_ROOT / "opheap-comparative-benchmark-output" / "comparative_summary.csv"
    env_path = Path(sys.argv[2]) if len(sys.argv) > 2 else REPO_ROOT / "opheap-comparative-benchmark-output" / "environment.json"
    out_path = Path(sys.argv[3]) if len(sys.argv) > 3 else REPO_ROOT / "docs" / "benchmarks-infographic.md"

    if not csv_path.exists():
        print(f"missing {csv_path} — run opheap_benchmark_comparative first", file=sys.stderr)
        return 1
    rows = load_rows(csv_path)
    env = json.loads(env_path.read_text()) if env_path.exists() else {}

    page = build_page(rows, env)
    # Splice the <style> block in right after the front matter.
    marker = "\n---\n"
    idx = page.index(marker) + len(marker)
    page = page[:idx] + INFOGRAPHIC_CSS + '\n<div class="infographic-root">\n' + page[idx:] + '\n</div>\n'

    out_path.write_text(page)
    print(f"wrote {out_path}")

    # A snapshot of the data backing the chart, published alongside it (the working-tree CSV
    # under opheap-comparative-benchmark-output/ is gitignored and not part of the published
    # docs/ site — see .github/workflows/pages.yml, source: ./docs).
    assets_csv = REPO_ROOT / "docs" / "assets" / "comparative_summary.csv"
    assets_csv.write_bytes(csv_path.read_bytes())
    print(f"wrote {assets_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
