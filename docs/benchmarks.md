---
layout: default
title: Benchmarks
---

# Benchmarking

The performance target is evidence-based: optimize persistent-object workloads under comparable
durability semantics rather than claiming that a durable commit is faster than another engine
running asynchronously. This page covers two things: a single-engine microbenchmark
(`opheap_benchmark`) and a comparative suite that runs the same workloads against opheap,
SQLite, SQLite through the `sqlite_orm` C++ ORM, LMDB and RocksDB — plus reference-only numbers
from SQLAlchemy (Python) and Diesel (Rust), kept clearly separate since they run in a different
process and language runtime.

See the [comparative infographic](benchmarks-infographic.md) for the Tier A results charted —
durability cost, bulk-load throughput, point-read latency, range-scan scaling and disk
footprint, all five engines side by side.

## Single-engine microbenchmark

```bash
cmake -S . -B build -DOPHEAP_BUILD_BENCHMARKS=ON
cmake --build build
./build/benchmarks/opheap_benchmark
```

Measures small durable transaction latency, checkpoint latency and close/reopen recovery
latency for opheap alone, writing `summary.csv` next to the temporary heap directory it creates.
Useful for a quick before/after check when changing opheap internals; use the comparative suite
below for anything that needs external context.

## Comparative suite methodology

1. **One shared, seeded dataset generator drives every adapter.** `benchmarks/adapters/dataset.hpp`
   is a splitmix64 PRNG (not `std::mt19937`, whose implementation isn't guaranteed identical
   across languages) seeded with a fixed constant (`0xC0FFEE`). Records are `{id, tag, seq,
   payload}`: `id` sequential, `tag = id % 10` (for the range-scan predicate), `seq` descending
   (for order-by), `payload` a random-length lowercase string. The Tier B reference scripts
   (`benchmarks/reference/*/`) port the same algorithm so the size *distribution* matches even
   where absolute row counts are scaled down for a script's runtime budget.

2. **Durability parity classes, always labeled.** Two named profiles are implemented by every
   adapter and recorded on every output row:

   | opheap `durability_mode` | SQLite | LMDB | RocksDB |
   |---|---|---|---|
   | `strict` ("durable") | `PRAGMA synchronous=FULL` + `journal_mode=WAL` | default (no `MDB_NOSYNC`) | `WriteOptions.sync=true` |
   | `relaxed` | `PRAGMA synchronous=OFF` | `MDB_NOSYNC \| MDB_NOMETASYNC` | `WriteOptions.sync=false` |

   Never compare opheap `strict` against another engine's non-durable mode as if equivalent —
   that's comparing a durable commit to an in-memory write.

   **This only holds on real persistent storage.** `/tmp` is tmpfs (RAM-backed) on many Linux
   systems, where `fsync` is nearly free — that collapses `durable` and `relaxed` into the same
   (meaningless) number. `opheap_benchmark_comparative` and the Tier B scripts deliberately
   write under the current working directory rather than the OS temp directory, and the C++
   driver checks `statfs()` and warns loudly if it still lands on tmpfs.

3. **One CSV schema for every adapter, in every language:**
   `engine,adapter_kind,workload,durability_profile,dataset_size,iterations,mean_us,p50_us,
   p95_us,p99_us,throughput_per_s,disk_bytes,checkpoint_us,recovery_us,notes`, where
   `adapter_kind` is one of `embedded-native`, `embedded-orm` or `cross-language-reference`. The
   `notes` column carries caveats (e.g. "no secondary index used") directly on the row they
   apply to.

4. **Workload matrix**, run for every `(engine, durability_profile)` pair, and for the
   size-dependent ones, every `(..., dataset_size)`:
   - `small_durable_txn` — single-record write + commit latency, 200 iterations (20 discarded as
     warm-up).
   - `bulk_load` — the whole dataset loaded in one commit, reported per-row.
   - `bulk_load_per_row_commit` — same dataset (size `small` only), one commit per row; the
     contrast against `bulk_load` isolates per-commit overhead (fsync, WAL record framing, ORM
     session overhead) from raw insert cost.
   - `point_read` — up to 300 reads sampled across the populated dataset.
   - `range_scan` — `tag == n ORDER BY seq LIMIT 50`, the lowest-common-denominator query every
     engine here can express. None of these adapters build a secondary index on `tag`, so this
     is a full scan with client-side filtering everywhere except SQL engines (SQLite/sqlite_orm
     issue it as a real indexless `WHERE`/`ORDER BY`/`LIMIT` query) — an intentional, honest
     baseline, not an optimized comparison. Iteration count shrinks as dataset size grows (20 /
     10 / 4 for small/medium/large) to keep full-scan cost from dominating total run time.
   - `checkpoint` — engine-specific flush-to-stable-storage operation (opheap `checkpoint()`,
     SQLite `PRAGMA wal_checkpoint(TRUNCATE)`, LMDB `mdb_env_sync`, RocksDB `Flush()`). These are
     genuinely different operations with different guarantees — see the per-engine notes in
     `benchmarks/adapters/*.hpp` — comparable as "cost of this engine's durability-barrier
     operation," not as identical mechanisms.
   - `recovery` — close then reopen, timing only the reopen half.
   - `mixed_read_write` — 200 operations, 80/20 read/write, dataset size `small`.

5. **Dataset size classes** (row count, payload byte range): `small` (1,000, 32–128), `medium`
   (10,000, 128–512), `large` (20,000, 256–1,024). The harness overrides the library default
   cache (`heap_config::cache_bytes` = 8 MiB) down to 2 MiB so `medium` and `large` reliably
   exceed it and exercise eviction/miss paths rather than fitting entirely resident.

6. **Warm-up discard**: the first N samples of each iterated workload are excluded from
   percentile/throughput computation (`opheap_bench::summarize`, see
   `benchmarks/adapters/benchmark_adapter.hpp`).

7. **Environment metadata captured automatically**: CPU model, hardware thread count, kernel,
   RAM, compiler, and each linked engine's version, written to `environment.json` next to the
   CSV — not something to reconstruct by hand before citing a number.

## Tier A — in-process C++ adapters

`benchmarks/adapters/`: a `backend` concept (not a virtual interface — dispatch overhead would
skew the very latencies being measured) implemented by `opheap_adapter.hpp`, `sqlite_adapter.hpp`
(raw SQLite3 C API), `sqlite_orm_adapter.hpp` (same SQLite engine through the `sqlite_orm`
header-only C++ ORM — the delta between these two isolates ORM overhead from engine cost),
`lmdb_adapter.hpp` and `rocksdb_adapter.hpp`. `benchmarks/comparative_main.cpp` is the driver.

```bash
cmake -S . -B build -DOPHEAP_BUILD_BENCHMARKS=ON -DOPHEAP_BUILD_BENCHMARKS_COMPARATIVE=ON
cmake --build build
./build/benchmarks/opheap_benchmark_comparative
```

Each engine is detected independently at configure time (`find_package`/`find_library`,
`sqlite_orm` vendored via `FetchContent` since it's header-only and not distro-packaged) and
skipped with a `message(STATUS ...)` line if not found — the same graceful-skip precedent as
`examples/beast_credentials/CMakeLists.txt` — so the suite degrades gracefully rather than being
all-or-nothing. Output goes to `./opheap-comparative-benchmark-output/` (relative to wherever the
binary is invoked from): `comparative_summary.csv` and `environment.json`.

## Tier B — cross-language reference

`benchmarks/reference/` — SQLAlchemy (Core and ORM) and Diesel, both over SQLite, tagged
`adapter_kind=cross-language-reference` on every row. **Not part of the fair comparison above**:
each runs in its own process and language runtime (Python's interpreter, a different SQLite
binding, Rust's compiled binary), overhead that isn't present for the in-process C++ adapters.
Useful for everyday context, not for "opheap is Nx faster than Django's ORM"-style claims. See
`benchmarks/reference/README.md` for setup and rationale, including why MongoDB is documented but
not yet implemented there.

## Merged report

```bash
python3 benchmarks/analyze.py
```

Reads every CSV it can find (the Tier A default location plus `benchmarks/reference/*/*.csv`, or
explicit paths passed on the command line) and renders one Markdown table per workload, with
Tier A and Tier B always in separate sections.

The [comparative infographic](benchmarks-infographic.md) is generated from the same Tier A CSV:

```bash
python3 benchmarks/render_infographic.py
```

Regenerates `docs/benchmarks-infographic.md` and refreshes the data snapshot published alongside
it at `docs/assets/comparative_summary.csv` — the working-tree CSV under
`opheap-comparative-benchmark-output/` is gitignored and isn't part of the published site, so the
snapshot is what backs the charts once deployed.

## Before citing a number externally

Record CPU, topology, RAM, storage medium, kernel, compiler, engine version, dataset size class,
thread count, transaction size and durability profile — `environment.json` covers most of this
automatically. State whether a number is Tier A (fair, in-process) or Tier B
(cross-language-reference, informative but not apples-to-apples).
