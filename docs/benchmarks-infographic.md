---
layout: default
title: Benchmarks infographic
description: opheap vs. SQLite, sqlite_orm, LMDB and RocksDB — comparative benchmark results at a glance.
---

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

<div class="infographic-root">

<h1>opheap vs. SQLite / sqlite_orm / LMDB / RocksDB</h1>

<p class="panel-note">Tier A of the <a href="benchmarks.md">comparative benchmark suite</a> —
same in-process C++ workloads, same durability profiles, same machine. Not shown here:
opheap wins on write-path simplicity (no schema, no ORM layer) and loses on anything that
benefits from a secondary index, which it doesn’t have yet (<a href="roadmap.md">roadmap
0.3</a>). Full per-workload data: <a href="assets/comparative_summary.csv">raw CSV</a>
&middot; regenerate with <code>python3 benchmarks/analyze.py</code> for a plain-text table.</p>

<p class="env-line">Intel(R) Core(TM) i7-14650HX · 24 threads · Linux 7.1.6-arch1-1 x86_64 · gcc 16.2.1 20260810 · SQLite 3.53.4 · LMDB LMDB 0.9.35: (Jan 27, 2026) · RocksDB 11.1.1</p>

<div class="legend"><span class="legend-item"><span class="legend-swatch swatch-opheap"></span>opheap</span><span class="legend-item"><span class="legend-swatch swatch-sqlite"></span>SQLite</span><span class="legend-item"><span class="legend-swatch swatch-sqlite_orm"></span>sqlite_orm</span><span class="legend-item"><span class="legend-swatch swatch-lmdb"></span>LMDB</span><span class="legend-item"><span class="legend-swatch swatch-rocksdb"></span>RocksDB</span></div>

<section class="panel"><h2>Durability cost: one record, one commit</h2><p class="panel-note">Same operation, two profiles. Durable commits cross a real fsync barrier; relaxed ones don’t — see the <a href="benchmarks.md">durability-profile mapping</a> for what each engine does under the hood.</p><div class="chart-pair">
    <figure class="chart-panel">
  <figcaption><h3>Durable (fsync every commit)</h3></figcaption>
  <svg class="chart" viewBox="0 0 319.0 234.0" role="img" aria-label="Single-record durable transaction latency by engine, microseconds, linear scale">
    <line class="grid" x1="40.0" y1="196.0" x2="309.0" y2="196.0"/><text class="axis-tick" x="36.0" y="199.0" text-anchor="end">0.0 µs</text><line class="grid" x1="40.0" y1="153.5" x2="309.0" y2="153.5"/><text class="axis-tick" x="36.0" y="156.5" text-anchor="end">821.8 µs</text><line class="grid" x1="40.0" y1="111.0" x2="309.0" y2="111.0"/><text class="axis-tick" x="36.0" y="114.0" text-anchor="end">1.64 ms</text><line class="grid" x1="40.0" y1="68.5" x2="309.0" y2="68.5"/><text class="axis-tick" x="36.0" y="71.5" text-anchor="end">2.47 ms</text><line class="grid" x1="40.0" y1="26.0" x2="309.0" y2="26.0"/><text class="axis-tick" x="36.0" y="29.0" text-anchor="end">3.29 ms</text>
    <line class="baseline" x1="40.0" y1="196.0" x2="309.0" y2="196.0"/>
    <path class="bar bar-opheap" d="M46.0,196.0 L46.0,119.7914239211451 Q46.0,115.7914239211451 50.0,115.7914239211451 L66.0,115.7914239211451 Q70.0,115.7914239211451 70.0,119.7914239211451 L70.0,196.0 Z"><title>opheap — 1.55 ms</title></path><text class="bar-value" x="58.0" y="109.8">1.55 ms</text><path class="bar bar-sqlite" d="M96.0,196.0 L96.0,153.10760712492964 Q96.0,149.10760712492964 100.0,149.10760712492964 L116.0,149.10760712492964 Q120.0,149.10760712492964 120.0,153.10760712492964 L120.0,196.0 Z"><title>SQLite — 906.7 µs</title></path><text class="bar-value" x="108.0" y="143.1">906.7 µs</text><path class="bar bar-sqlite_orm" d="M146.0,196.0 L146.0,30.0 Q146.0,26.0 150.0,26.0 L166.0,26.0 Q170.0,26.0 170.0,30.0 L170.0,196.0 Z"><title>sqlite_orm — 3.29 ms</title></path><text class="bar-value" x="158.0" y="20.0">3.29 ms</text><path class="bar bar-lmdb" d="M196.0,196.0 L196.0,157.1049512480796 Q196.0,153.1049512480796 200.0,153.1049512480796 L216.0,153.1049512480796 Q220.0,153.1049512480796 220.0,157.1049512480796 L220.0,196.0 Z"><title>LMDB — 829.4 µs</title></path><text class="bar-value" x="208.0" y="147.1">829.4 µs</text><path class="bar bar-rocksdb" d="M246.0,196.0 L246.0,154.6408420924537 Q246.0,150.6408420924537 250.0,150.6408420924537 L266.0,150.6408420924537 Q270.0,150.6408420924537 270.0,154.6408420924537 L270.0,196.0 Z"><title>RocksDB — 877.0 µs</title></path><text class="bar-value" x="258.0" y="144.6">877.0 µs</text>
    <text class="bar-cat" x="58.0" y="216.0">opheap</text><text class="bar-cat" x="108.0" y="216.0">SQLite</text><text class="bar-cat" x="158.0" y="216.0">sqlite_orm</text><text class="bar-cat" x="208.0" y="216.0">LMDB</text><text class="bar-cat" x="258.0" y="216.0">RocksDB</text>
    
  </svg>
</figure>
    <figure class="chart-panel">
  <figcaption><h3>Relaxed (buffered)</h3></figcaption>
  <svg class="chart" viewBox="0 0 319.0 234.0" role="img" aria-label="Single-record relaxed transaction latency by engine, microseconds, linear scale">
    <line class="grid" x1="40.0" y1="196.0" x2="309.0" y2="196.0"/><text class="axis-tick" x="36.0" y="199.0" text-anchor="end">0.0 µs</text><line class="grid" x1="40.0" y1="153.5" x2="309.0" y2="153.5"/><text class="axis-tick" x="36.0" y="156.5" text-anchor="end">58.9 µs</text><line class="grid" x1="40.0" y1="111.0" x2="309.0" y2="111.0"/><text class="axis-tick" x="36.0" y="114.0" text-anchor="end">117.7 µs</text><line class="grid" x1="40.0" y1="68.5" x2="309.0" y2="68.5"/><text class="axis-tick" x="36.0" y="71.5" text-anchor="end">176.6 µs</text><line class="grid" x1="40.0" y1="26.0" x2="309.0" y2="26.0"/><text class="axis-tick" x="36.0" y="29.0" text-anchor="end">235.4 µs</text>
    <line class="baseline" x1="40.0" y1="196.0" x2="309.0" y2="196.0"/>
    <path class="bar bar-opheap" d="M46.0,196.0 L46.0,63.65495063805204 Q46.0,59.65495063805204 50.0,59.65495063805204 L66.0,59.65495063805204 Q70.0,59.65495063805204 70.0,63.65495063805204 L70.0,196.0 Z"><title>opheap — 188.8 µs</title></path><text class="bar-value" x="58.0" y="53.7">188.8 µs</text><path class="bar bar-sqlite" d="M96.0,196.0 L96.0,190.74035700327946 Q96.0,186.74035700327946 100.0,186.74035700327946 L116.0,186.74035700327946 Q120.0,186.74035700327946 120.0,190.74035700327946 L120.0,196.0 Z"><title>SQLite — 12.8 µs</title></path><text class="bar-value" x="108.0" y="180.7">12.8 µs</text><path class="bar bar-sqlite_orm" d="M146.0,196.0 L146.0,30.0 Q146.0,26.0 150.0,26.0 L166.0,26.0 Q170.0,26.0 170.0,30.0 L170.0,196.0 Z"><title>sqlite_orm — 235.4 µs</title></path><text class="bar-value" x="158.0" y="20.0">235.4 µs</text><path class="bar bar-lmdb" d="M196.0,196.0 L196.0,190.26697932065724 Q196.0,186.26697932065724 200.0,186.26697932065724 L216.0,186.26697932065724 Q220.0,186.26697932065724 220.0,190.26697932065724 L220.0,196.0 Z"><title>LMDB — 13.5 µs</title></path><text class="bar-value" x="208.0" y="180.3">13.5 µs</text><path class="bar bar-rocksdb" d="M246.0,196.0 L246.0,193.13858005811286 Q246.0,189.13858005811286 250.0,189.13858005811286 L266.0,189.13858005811286 Q270.0,189.13858005811286 270.0,193.13858005811286 L270.0,196.0 Z"><title>RocksDB — 9.5 µs</title></path><text class="bar-value" x="258.0" y="183.1">9.5 µs</text>
    <text class="bar-cat" x="58.0" y="216.0">opheap</text><text class="bar-cat" x="108.0" y="216.0">SQLite</text><text class="bar-cat" x="158.0" y="216.0">sqlite_orm</text><text class="bar-cat" x="208.0" y="216.0">LMDB</text><text class="bar-cat" x="258.0" y="216.0">RocksDB</text>
    
  </svg>
</figure>
  </div></section>
<section class="panel"><h2>Bulk load throughput</h2><p class="panel-note">20,000 rows, one commit, durable profile. Log scale — the gap between the embedded KV/SQL engines and opheap’s per-root transaction model is real, not a rounding artifact.</p><figure class="chart-panel">
  <figcaption><h3>Rows/second, large dataset (log scale)</h3></figcaption>
  <svg class="chart" viewBox="0 0 319.0 234.0" role="img" aria-label="Bulk load throughput by engine, rows per second, log scale">
    <line class="grid" x1="40.0" y1="194.0" x2="309.0" y2="194.0"/><text class="axis-tick" x="36.0" y="197.0" text-anchor="end">5.8K/s</text><line class="grid" x1="40.0" y1="153.5" x2="309.0" y2="153.5"/><text class="axis-tick" x="36.0" y="156.5" text-anchor="end">14.8K/s</text><line class="grid" x1="40.0" y1="111.0" x2="309.0" y2="111.0"/><text class="axis-tick" x="36.0" y="114.0" text-anchor="end">38.1K/s</text><line class="grid" x1="40.0" y1="68.5" x2="309.0" y2="68.5"/><text class="axis-tick" x="36.0" y="71.5" text-anchor="end">97.9K/s</text><line class="grid" x1="40.0" y1="26.0" x2="309.0" y2="26.0"/><text class="axis-tick" x="36.0" y="29.0" text-anchor="end">251.3K/s</text>
    <line class="baseline" x1="40.0" y1="196.0" x2="309.0" y2="196.0"/>
    <path class="bar bar-opheap" d="M46.0,196.0 L46.0,196.0 Q46.0,194.0 48.0,194.0 L68.0,194.0 Q70.0,194.0 70.0,196.0 L70.0,196.0 Z"><title>opheap — 5.8K/s</title></path><text class="bar-value" x="58.0" y="188.0">5.8K/s</text><path class="bar bar-sqlite" d="M96.0,196.0 L96.0,30.0 Q96.0,26.0 100.0,26.0 L116.0,26.0 Q120.0,26.0 120.0,30.0 L120.0,196.0 Z"><title>SQLite — 251.3K/s</title></path><text class="bar-value" x="108.0" y="20.0">251.3K/s</text><path class="bar bar-sqlite_orm" d="M146.0,196.0 L146.0,79.0574332299683 Q146.0,75.0574332299683 150.0,75.0574332299683 L166.0,75.0574332299683 Q170.0,75.0574332299683 170.0,79.0574332299683 L170.0,196.0 Z"><title>sqlite_orm — 84.6K/s</title></path><text class="bar-value" x="158.0" y="69.1">84.6K/s</text><path class="bar bar-lmdb" d="M196.0,196.0 L196.0,31.9025343766846 Q196.0,27.9025343766846 200.0,27.9025343766846 L216.0,27.9025343766846 Q220.0,27.9025343766846 220.0,31.9025343766846 L220.0,196.0 Z"><title>LMDB — 240.9K/s</title></path><text class="bar-value" x="208.0" y="21.9">240.9K/s</text><path class="bar bar-rocksdb" d="M246.0,196.0 L246.0,43.545256942702 Q246.0,39.545256942702 250.0,39.545256942702 L266.0,39.545256942702 Q270.0,39.545256942702 270.0,43.545256942702 L270.0,196.0 Z"><title>RocksDB — 186.1K/s</title></path><text class="bar-value" x="258.0" y="33.5">186.1K/s</text>
    <text class="bar-cat" x="58.0" y="216.0">opheap</text><text class="bar-cat" x="108.0" y="216.0">SQLite</text><text class="bar-cat" x="158.0" y="216.0">sqlite_orm</text><text class="bar-cat" x="208.0" y="216.0">LMDB</text><text class="bar-cat" x="258.0" y="216.0">RocksDB</text>
    
  </svg>
</figure></section>
<section class="panel"><h2>Point read latency</h2><p class="panel-note">300 sampled reads across a 20,000-row dataset, durable profile, log scale.</p><figure class="chart-panel">
  <figcaption><h3>Microseconds per read, large dataset (log scale)</h3></figcaption>
  <svg class="chart" viewBox="0 0 319.0 234.0" role="img" aria-label="Point read latency by engine, microseconds, log scale">
    <line class="grid" x1="40.0" y1="194.0" x2="309.0" y2="194.0"/><text class="axis-tick" x="36.0" y="197.0" text-anchor="end">4.6 µs</text><line class="grid" x1="40.0" y1="153.5" x2="309.0" y2="153.5"/><text class="axis-tick" x="36.0" y="156.5" text-anchor="end">11.1 µs</text><line class="grid" x1="40.0" y1="111.0" x2="309.0" y2="111.0"/><text class="axis-tick" x="36.0" y="114.0" text-anchor="end">27.0 µs</text><line class="grid" x1="40.0" y1="68.5" x2="309.0" y2="68.5"/><text class="axis-tick" x="36.0" y="71.5" text-anchor="end">65.6 µs</text><line class="grid" x1="40.0" y1="26.0" x2="309.0" y2="26.0"/><text class="axis-tick" x="36.0" y="29.0" text-anchor="end">159.6 µs</text>
    <line class="baseline" x1="40.0" y1="196.0" x2="309.0" y2="196.0"/>
    <path class="bar bar-opheap" d="M46.0,196.0 L46.0,56.95604845195365 Q46.0,52.95604845195365 50.0,52.95604845195365 L66.0,52.95604845195365 Q70.0,52.95604845195365 70.0,56.95604845195365 L70.0,196.0 Z"><title>opheap — 90.8 µs</title></path><text class="bar-value" x="58.0" y="47.0">90.8 µs</text><path class="bar bar-sqlite" d="M96.0,196.0 L96.0,190.71320588296146 Q96.0,186.71320588296146 100.0,186.71320588296146 L116.0,186.71320588296146 Q120.0,186.71320588296146 120.0,190.71320588296146 L120.0,196.0 Z"><title>SQLite — 5.5 µs</title></path><text class="bar-value" x="108.0" y="180.7">5.5 µs</text><path class="bar bar-sqlite_orm" d="M146.0,196.0 L146.0,30.0 Q146.0,26.0 150.0,26.0 L166.0,26.0 Q170.0,26.0 170.0,30.0 L170.0,196.0 Z"><title>sqlite_orm — 159.6 µs</title></path><text class="bar-value" x="158.0" y="20.0">159.6 µs</text><path class="bar bar-lmdb" d="M196.0,196.0 L196.0,196.0 Q196.0,194.0 198.0,194.0 L218.0,194.0 Q220.0,194.0 220.0,196.0 L220.0,196.0 Z"><title>LMDB — 4.6 µs</title></path><text class="bar-value" x="208.0" y="188.0">4.6 µs</text><path class="bar bar-rocksdb" d="M246.0,196.0 L246.0,177.71606436304245 Q246.0,173.71606436304245 250.0,173.71606436304245 L266.0,173.71606436304245 Q270.0,173.71606436304245 270.0,177.71606436304245 L270.0,196.0 Z"><title>RocksDB — 7.3 µs</title></path><text class="bar-value" x="258.0" y="167.7">7.3 µs</text>
    <text class="bar-cat" x="58.0" y="216.0">opheap</text><text class="bar-cat" x="108.0" y="216.0">SQLite</text><text class="bar-cat" x="158.0" y="216.0">sqlite_orm</text><text class="bar-cat" x="208.0" y="216.0">LMDB</text><text class="bar-cat" x="258.0" y="216.0">RocksDB</text>
    
  </svg>
</figure></section>
<section class="panel"><h2>Range scan cost grows with dataset size</h2><p class="panel-note">Equality filter + order + limit, no secondary index on any engine — the lowest-common-denominator query every adapter can express (see <a href="benchmarks.md">methodology</a>). Log scale: opheap’s per-root transaction model has no index to lean on, so its cost scales with row count far faster than the others’.</p><figure class="chart-panel">
  <figcaption><h3>Microseconds per scan (log scale)</h3></figcaption>
  <svg class="chart chart-wide" viewBox="0 0 859.0 252.0" role="img" aria-label="Range scan latency by engine and dataset size, microseconds, log scale">
    <line class="grid" x1="40.0" y1="194.0" x2="849.0" y2="194.0"/><text class="axis-tick" x="36.0" y="197.0" text-anchor="end">361.2 µs</text><line class="grid" x1="40.0" y1="153.5" x2="849.0" y2="153.5"/><text class="axis-tick" x="36.0" y="156.5" text-anchor="end">3.04 ms</text><line class="grid" x1="40.0" y1="111.0" x2="849.0" y2="111.0"/><text class="axis-tick" x="36.0" y="114.0" text-anchor="end">25.66 ms</text><line class="grid" x1="40.0" y1="68.5" x2="849.0" y2="68.5"/><text class="axis-tick" x="36.0" y="71.5" text-anchor="end">216.27 ms</text><line class="grid" x1="40.0" y1="26.0" x2="849.0" y2="26.0"/><text class="axis-tick" x="36.0" y="29.0" text-anchor="end">1.82 s</text>
    <line class="baseline" x1="40.0" y1="196.0" x2="849.0" y2="196.0"/>
    <path class="bar bar-opheap" d="M46.0,196.0 L46.0,99.62396384857217 Q46.0,95.62396384857217 50.0,95.62396384857217 L66.0,95.62396384857217 Q70.0,95.62396384857217 70.0,99.62396384857217 L70.0,196.0 Z"><title>opheap — 55.49 ms</title></path><text class="bar-value" x="58.0" y="89.6">55.49 ms</text><path class="bar bar-sqlite" d="M96.0,196.0 L96.0,196.0 Q96.0,194.0 98.0,194.0 L118.0,194.0 Q120.0,194.0 120.0,196.0 L120.0,196.0 Z"><title>SQLite — 361.2 µs</title></path><text class="bar-value" x="108.0" y="188.0">361.2 µs</text><path class="bar bar-sqlite_orm" d="M146.0,196.0 L146.0,186.99253983510735 Q146.0,182.99253983510735 150.0,182.99253983510735 L166.0,182.99253983510735 Q170.0,182.99253983510735 170.0,186.99253983510735 L170.0,196.0 Z"><title>sqlite_orm — 693.6 µs</title></path><text class="bar-value" x="158.0" y="177.0">693.6 µs</text><path class="bar bar-lmdb" d="M196.0,196.0 L196.0,178.41003724117812 Q196.0,174.41003724117812 200.0,174.41003724117812 L216.0,174.41003724117812 Q220.0,174.41003724117812 220.0,178.41003724117812 L220.0,196.0 Z"><title>LMDB — 1.07 ms</title></path><text class="bar-value" x="208.0" y="168.4">1.07 ms</text><path class="bar bar-rocksdb" d="M246.0,196.0 L246.0,171.0513788162639 Q246.0,167.0513788162639 250.0,167.0513788162639 L266.0,167.0513788162639 Q270.0,167.0513788162639 270.0,171.0513788162639 L270.0,196.0 Z"><title>RocksDB — 1.54 ms</title></path><text class="bar-value" x="258.0" y="161.1">1.54 ms</text><path class="bar bar-opheap" d="M316.0,196.0 L316.0,46.288771504719705 Q316.0,42.288771504719705 320.0,42.288771504719705 L336.0,42.288771504719705 Q340.0,42.288771504719705 340.0,46.288771504719705 L340.0,196.0 Z"><title>opheap — 805.25 ms</title></path><text class="bar-value" x="328.0" y="36.3">805.25 ms</text><path class="bar bar-sqlite" d="M366.0,196.0 L366.0,135.95225319457205 Q366.0,131.95225319457205 370.0,131.95225319457205 L386.0,131.95225319457205 Q390.0,131.95225319457205 390.0,135.95225319457205 L390.0,196.0 Z"><title>SQLite — 8.97 ms</title></path><text class="bar-value" x="378.0" y="126.0">8.97 ms</text><path class="bar bar-sqlite_orm" d="M416.0,196.0 L416.0,144.79123383656787 Q416.0,140.79123383656787 420.0,140.79123383656787 L436.0,140.79123383656787 Q440.0,140.79123383656787 440.0,144.79123383656787 L440.0,196.0 Z"><title>sqlite_orm — 5.76 ms</title></path><text class="bar-value" x="428.0" y="134.8">5.76 ms</text><path class="bar bar-lmdb" d="M466.0,196.0 L466.0,127.93756337844724 Q466.0,123.93756337844724 470.0,123.93756337844724 L486.0,123.93756337844724 Q490.0,123.93756337844724 490.0,127.93756337844724 L490.0,196.0 Z"><title>LMDB — 13.41 ms</title></path><text class="bar-value" x="478.0" y="117.9">13.41 ms</text><path class="bar bar-rocksdb" d="M516.0,196.0 L516.0,124.55093441117582 Q516.0,120.55093441117582 520.0,120.55093441117582 L536.0,120.55093441117582 Q540.0,120.55093441117582 540.0,124.55093441117582 L540.0,196.0 Z"><title>RocksDB — 15.89 ms</title></path><text class="bar-value" x="528.0" y="114.6">15.89 ms</text><path class="bar bar-opheap" d="M586.0,196.0 L586.0,30.0 Q586.0,26.0 590.0,26.0 L606.0,26.0 Q610.0,26.0 610.0,30.0 L610.0,196.0 Z"><title>opheap — 1.82 s</title></path><text class="bar-value" x="598.0" y="20.0">1.82 s</text><path class="bar bar-sqlite" d="M636.0,196.0 L636.0,114.65631546546284 Q636.0,110.65631546546284 640.0,110.65631546546284 L656.0,110.65631546546284 Q660.0,110.65631546546284 660.0,114.65631546546284 L660.0,196.0 Z"><title>SQLite — 26.11 ms</title></path><text class="bar-value" x="648.0" y="104.7">26.11 ms</text><path class="bar bar-sqlite_orm" d="M686.0,196.0 L686.0,126.5028250869219 Q686.0,122.5028250869219 690.0,122.5028250869219 L706.0,122.5028250869219 Q710.0,122.5028250869219 710.0,126.5028250869219 L710.0,196.0 Z"><title>sqlite_orm — 14.41 ms</title></path><text class="bar-value" x="698.0" y="116.5">14.41 ms</text><path class="bar bar-lmdb" d="M736.0,196.0 L736.0,111.98043370618024 Q736.0,107.98043370618024 740.0,107.98043370618024 L756.0,107.98043370618024 Q760.0,107.98043370618024 760.0,111.98043370618024 L760.0,196.0 Z"><title>LMDB — 29.86 ms</title></path><text class="bar-value" x="748.0" y="102.0">29.86 ms</text><path class="bar bar-rocksdb" d="M786.0,196.0 L786.0,110.56727654062135 Q786.0,106.56727654062135 790.0,106.56727654062135 L806.0,106.56727654062135 Q810.0,106.56727654062135 810.0,110.56727654062135 L810.0,196.0 Z"><title>RocksDB — 32.05 ms</title></path><text class="bar-value" x="798.0" y="100.6">32.05 ms</text>
    <text class="bar-cat" x="58.0" y="216.0">opheap</text><text class="bar-cat" x="108.0" y="216.0">SQLite</text><text class="bar-cat" x="158.0" y="216.0">sqlite_orm</text><text class="bar-cat" x="208.0" y="216.0">LMDB</text><text class="bar-cat" x="258.0" y="216.0">RocksDB</text><text class="bar-cat" x="328.0" y="216.0">opheap</text><text class="bar-cat" x="378.0" y="216.0">SQLite</text><text class="bar-cat" x="428.0" y="216.0">sqlite_orm</text><text class="bar-cat" x="478.0" y="216.0">LMDB</text><text class="bar-cat" x="528.0" y="216.0">RocksDB</text><text class="bar-cat" x="598.0" y="216.0">opheap</text><text class="bar-cat" x="648.0" y="216.0">SQLite</text><text class="bar-cat" x="698.0" y="216.0">sqlite_orm</text><text class="bar-cat" x="748.0" y="216.0">LMDB</text><text class="bar-cat" x="798.0" y="216.0">RocksDB</text>
    <text class="group-label" x="158.0" y="234.0">Small</text><text class="group-label" x="428.0" y="234.0">Medium</text><text class="group-label" x="698.0" y="234.0">Large</text>
  </svg>
</figure></section>
<section class="panel"><h2>Disk footprint (20,000-row dataset)</h2><p class="panel-note">Bytes on disk after a checkpoint, durable profile.</p><div class="stat-row"><div class="stat-tile">
      <span class="stat-accent swatch-rocksdb"></span>
      <div class="stat-label">RocksDB</div>
      <div class="stat-value">13 MB</div>
    </div><div class="stat-tile">
      <span class="stat-accent swatch-sqlite_orm"></span>
      <div class="stat-label">sqlite_orm</div>
      <div class="stat-value">14 MB</div>
    </div><div class="stat-tile">
      <span class="stat-accent swatch-sqlite"></span>
      <div class="stat-label">SQLite</div>
      <div class="stat-value">14 MB</div>
    </div><div class="stat-tile">
      <span class="stat-accent swatch-opheap"></span>
      <div class="stat-label">opheap</div>
      <div class="stat-value">15 MB</div>
    </div><div class="stat-tile">
      <span class="stat-accent swatch-lmdb"></span>
      <div class="stat-label">LMDB</div>
      <div class="stat-value">18 MB</div>
    </div></div></section>

<p class="panel-note">Cross-language reference numbers (SQLAlchemy, Diesel) exist too, but aren’t
charted here — they run in a different process/runtime and aren’t apples-to-apples with the
in-process C++ tier above. See <a href="benchmarks.md">Tier B</a>.</p>

</div>
