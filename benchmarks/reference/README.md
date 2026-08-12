# Tier B — cross-language reference benchmarks

This directory holds **reference-only** numbers. They are not part of the fair, apples-to-apples
comparison in `benchmarks/adapters/` (Tier A, see `../../docs/benchmarks.md`). Each script here
runs in its own process and language runtime — Python's interpreter, SQLAlchemy's Python-level
ORM machinery, Rust's compiled binary via a different SQLite binding — none of which is present
for the in-process C++ Tier A adapters. Every row these scripts write is tagged
`adapter_kind=cross-language-reference` in the shared CSV schema precisely so it can't be
mistaken for a fair comparison against opheap/SQLite/sqlite_orm/LMDB/RocksDB.

What they *are* useful for: showing what the equivalent workload costs through the tools most
people actually reach for day to day (an ORM in a scripting language, an ORM in another compiled
language), and giving opheap's numbers some everyday context beyond the embedded-C++ world.

## Methodology

Same rules as Tier A (see `../../docs/benchmarks.md` for full detail):

- a seeded, deterministic dataset generator (splitmix64 — a port of
  `../adapters/dataset.hpp`, same seed/algorithm, so the size *distribution* matches even
  where absolute row counts are scaled down for a script's runtime budget);
- both `durable` (fsync/sync every commit) and `relaxed` (buffered) durability profiles,
  labeled on every row;
- the same CSV schema (`engine,adapter_kind,workload,durability_profile,dataset_size,iterations,
  mean_us,p50_us,p95_us,p99_us,throughput_per_s,disk_bytes,checkpoint_us,recovery_us,notes`);
- output written under the current working directory, never the default temp directory — on
  many Linux systems `/tmp` is tmpfs (RAM-backed), where `fsync` is nearly free, which would
  silently collapse the `durable`/`relaxed` distinction into noise.

## Directories

- `python_sqlalchemy/` — SQLAlchemy Core (raw query builder) and SQLAlchemy ORM (declarative
  session), both over SQLite. **Implemented and verified.**
- `rust_diesel/` — Diesel over SQLite. **Implemented and verified.**
- `mongodb/` — **not implemented yet.** Running it needs a local `mongod` (e.g. via Docker),
  which this repo's automation won't start on its own; see `mongodb/README.md` for the plan.

## Running

```bash
cd benchmarks/reference/python_sqlalchemy
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python bench.py sqlalchemy_summary.csv

cd ../rust_diesel
cargo run --release -- diesel_summary.csv
```

Then merge everything (Tier A + whichever Tier B CSVs exist) into one report:

```bash
python3 ../../analyze.py ../../../opheap-comparative-benchmark-output/comparative_summary.csv \
    python_sqlalchemy/sqlalchemy_summary.csv rust_diesel/diesel_summary.csv
```

(Run with no arguments, `analyze.py` also auto-discovers the default Tier A CSV location plus
any `benchmarks/reference/*/*.csv` files.)
