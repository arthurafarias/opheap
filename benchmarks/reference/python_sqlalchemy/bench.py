#!/usr/bin/env python3
"""Tier B cross-language reference: SQLAlchemy Core and SQLAlchemy ORM over SQLite.

Reference-only — see benchmarks/reference/README.md and docs/benchmarks.md. This runs in a
separate process/runtime from the Tier A C++ adapters, so its numbers include Python interpreter
and SQLAlchemy overhead that isn't present for opheap/SQLite/sqlite_orm/LMDB/RocksDB; every row
this script writes is tagged adapter_kind=cross-language-reference precisely so it never gets
mistaken for a fair, apples-to-apples comparison against Tier A.

Dataset sizes here are intentionally smaller than benchmarks/adapters/dataset.hpp's (see
SIZE_CLASSES below) to keep a manual run's wall time reasonable given Python/ORM overhead — the
point of this tier is cross-language texture, not precise cross-language scaling.

The record generator is a Python port of the splitmix64 generator in
benchmarks/adapters/dataset.hpp (same seed, same algorithm) so the size *distribution* matches
what the C++ generator would produce, even though the row counts differ.

Usage:
    python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
    .venv/bin/python bench.py [output_csv]
"""

from __future__ import annotations

import csv
import shutil
import statistics
import sys
import time
from dataclasses import dataclass
from pathlib import Path

from sqlalchemy import Column, Integer, String, MetaData, Table, create_engine, select, text
from sqlalchemy.orm import DeclarativeBase, Session, sessionmaker

MASK64 = (1 << 64) - 1
DATASET_SEED = 0xC0FFEE
TAG_CARDINALITY = 10

# Smaller than benchmarks/adapters/dataset.hpp's classes — see module docstring.
SIZE_CLASSES = {
    "small": (300, 32, 128),
    "medium": (2000, 128, 512),
    "large": (8000, 256, 1024),
}

CSV_COLUMNS = [
    "engine", "adapter_kind", "workload", "durability_profile", "dataset_size", "iterations",
    "mean_us", "p50_us", "p95_us", "p99_us", "throughput_per_s", "disk_bytes", "checkpoint_us",
    "recovery_us", "notes",
]


class SplitMix64:
    """Port of opheap_bench::splitmix64 (benchmarks/adapters/dataset.hpp)."""

    def __init__(self, seed: int):
        self.state = seed & MASK64

    def next(self) -> int:
        self.state = (self.state + 0x9E3779B97F4A7C15) & MASK64
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK64
        return z ^ (z >> 31)

    def range(self, lo: int, hi: int) -> int:
        return lo + self.next() % (hi - lo + 1)


@dataclass
class Record:
    id: int
    tag: int
    seq: int
    payload: str


def generate_dataset(size_name: str, seed: int = DATASET_SEED) -> list[Record]:
    row_count, min_bytes, max_bytes = SIZE_CLASSES[size_name]
    rng = SplitMix64(seed)
    rows = []
    for i in range(row_count):
        length = rng.range(min_bytes, max_bytes)
        payload = "".join(chr(ord("a") + rng.next() % 26) for _ in range(length))
        rows.append(Record(id=i, tag=i % TAG_CARDINALITY, seq=row_count - i, payload=payload))
    return rows


def percentile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    values = sorted(values)
    index = int(p * (len(values) - 1))
    return values[index]


def summarize(samples: list[float], warmup: int = 0) -> dict:
    trimmed = samples[warmup:] if len(samples) > warmup else samples
    mean = statistics.fmean(trimmed) if trimmed else 0.0
    return {
        "mean_us": mean, "p50_us": percentile(trimmed, 0.50), "p95_us": percentile(trimmed, 0.95),
        "p99_us": percentile(trimmed, 0.99), "throughput_per_s": (1_000_000.0 / mean) if mean else 0.0,
    }


def aggregate_summary(total_us: float, ops: int) -> dict:
    mean = total_us / ops if ops else 0.0
    return {"mean_us": mean, "p50_us": mean, "p95_us": mean, "p99_us": mean,
            "throughput_per_s": (1_000_000.0 / mean) if mean else 0.0}


class CsvWriter:
    def __init__(self, path: Path):
        self.file = path.open("w", newline="")
        self.writer = csv.writer(self.file)
        self.writer.writerow(CSV_COLUMNS)

    def write(self, row: dict) -> None:
        self.writer.writerow([row.get(c, "") for c in CSV_COLUMNS])
        self.file.flush()

    def close(self) -> None:
        self.file.close()


def disk_bytes(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(f.stat().st_size for f in path.rglob("*") if f.is_file())


class Base(DeclarativeBase):
    pass


class OrmRecord(Base):
    __tablename__ = "records"
    id = Column(Integer, primary_key=True)
    tag = Column(Integer, nullable=False)
    seq = Column(Integer, nullable=False)
    payload = Column(String, nullable=False)


class CoreBackend:
    engine_name = "sqlalchemy_core"

    def __init__(self):
        self.metadata = MetaData()
        self.table = Table(
            "records", self.metadata,
            Column("id", Integer, primary_key=True),
            Column("tag", Integer, nullable=False),
            Column("seq", Integer, nullable=False),
            Column("payload", String, nullable=False),
        )
        self.sql_engine = None
        self.path = None
        self.profile = None

    def open(self, path: Path, profile: str) -> None:
        self.path = path
        path.mkdir(parents=True, exist_ok=True)
        self.profile = profile
        self.sql_engine = create_engine(f"sqlite:///{path / 'records.sqlite3'}", future=True)
        with self.sql_engine.begin() as conn:
            conn.execute(text("PRAGMA journal_mode=WAL"))
            conn.execute(text(f"PRAGMA synchronous={'FULL' if profile == 'durable' else 'OFF'}"))
        self.metadata.create_all(self.sql_engine)

    def put_commit(self, r: Record) -> None:
        with self.sql_engine.begin() as conn:
            conn.execute(self.table.insert().prefix_with("OR REPLACE"),
                         {"id": r.id, "tag": r.tag, "seq": r.seq, "payload": r.payload})

    def bulk_load(self, rows: list[Record], commit_every: int) -> None:
        for start in range(0, len(rows), commit_every):
            chunk = rows[start:start + commit_every]
            with self.sql_engine.begin() as conn:
                conn.execute(self.table.insert().prefix_with("OR REPLACE"),
                             [{"id": r.id, "tag": r.tag, "seq": r.seq, "payload": r.payload} for r in chunk])

    def get(self, id_: int) -> Record | None:
        with self.sql_engine.connect() as conn:
            row = conn.execute(select(self.table).where(self.table.c.id == id_)).first()
        return None if row is None else Record(row.id, row.tag, row.seq, row.payload)

    def range_scan(self, tag: int, limit: int) -> list[Record]:
        with self.sql_engine.connect() as conn:
            result = conn.execute(
                select(self.table).where(self.table.c.tag == tag).order_by(self.table.c.seq).limit(limit))
            return [Record(r.id, r.tag, r.seq, r.payload) for r in result]

    def checkpoint(self) -> None:
        with self.sql_engine.begin() as conn:
            conn.execute(text("PRAGMA wal_checkpoint(TRUNCATE)"))

    def close(self) -> None:
        if self.sql_engine:
            self.sql_engine.dispose()
        self.sql_engine = None

    def reopen(self) -> None:
        self.open(self.path, self.profile)

    def disk_bytes(self) -> int:
        return disk_bytes(self.path)


class OrmBackend:
    engine_name = "sqlalchemy_orm"

    def __init__(self):
        self.sql_engine = None
        self.session_factory = None
        self.path = None
        self.profile = None

    def open(self, path: Path, profile: str) -> None:
        self.path = path
        path.mkdir(parents=True, exist_ok=True)
        self.profile = profile
        self.sql_engine = create_engine(f"sqlite:///{path / 'records_orm.sqlite3'}", future=True)
        with self.sql_engine.begin() as conn:
            conn.execute(text("PRAGMA journal_mode=WAL"))
            conn.execute(text(f"PRAGMA synchronous={'FULL' if profile == 'durable' else 'OFF'}"))
        Base.metadata.create_all(self.sql_engine)
        self.session_factory = sessionmaker(bind=self.sql_engine, future=True)

    def put_commit(self, r: Record) -> None:
        with self.session_factory() as session:
            session.merge(OrmRecord(id=r.id, tag=r.tag, seq=r.seq, payload=r.payload))
            session.commit()

    def bulk_load(self, rows: list[Record], commit_every: int) -> None:
        for start in range(0, len(rows), commit_every):
            chunk = rows[start:start + commit_every]
            with self.session_factory() as session:
                session.add_all([OrmRecord(id=r.id, tag=r.tag, seq=r.seq, payload=r.payload) for r in chunk])
                session.commit()

    def get(self, id_: int) -> Record | None:
        with self.session_factory() as session:
            row = session.get(OrmRecord, id_)
            return None if row is None else Record(row.id, row.tag, row.seq, row.payload)

    def range_scan(self, tag: int, limit: int) -> list[Record]:
        with self.session_factory() as session:
            rows = (session.query(OrmRecord).filter(OrmRecord.tag == tag)
                    .order_by(OrmRecord.seq).limit(limit).all())
            return [Record(r.id, r.tag, r.seq, r.payload) for r in rows]

    def checkpoint(self) -> None:
        with self.sql_engine.begin() as conn:
            conn.execute(text("PRAGMA wal_checkpoint(TRUNCATE)"))

    def close(self) -> None:
        if self.sql_engine:
            self.sql_engine.dispose()
        self.sql_engine = None

    def reopen(self) -> None:
        self.open(self.path, self.profile)

    def disk_bytes(self) -> int:
        return disk_bytes(self.path)


SMALL_TXN_ITERATIONS = 100
SMALL_TXN_WARMUP = 10
POINT_READ_ITERATIONS = 100
POINT_READ_WARMUP = 10
CHECKPOINT_ITERATIONS = 3
RECOVERY_ITERATIONS = 3
MIXED_ITERATIONS = 100
MIXED_WARMUP = 10


def range_scan_iterations_for(size_name: str) -> int:
    return {"small": 10, "medium": 5, "large": 2}[size_name]


def fresh_dir(root: Path, label: str) -> Path:
    d = root / label
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True, exist_ok=True)
    return d


def elapsed_us(start: float) -> float:
    return (time.perf_counter() - start) * 1_000_000.0


def run_bulk_read_query_lifecycle(csv_writer: CsvWriter, backend_factory, scratch: Path, profile: str,
                                   size_name: str) -> None:
    backend = backend_factory()
    backend.open(fresh_dir(scratch, size_name), profile)
    rows = generate_dataset(size_name)

    start = time.perf_counter()
    backend.bulk_load(rows, len(rows))
    total_us = elapsed_us(start)
    csv_writer.write({"engine": backend.engine_name, "adapter_kind": "cross-language-reference",
                       "workload": "bulk_load", "durability_profile": profile, "dataset_size": size_name,
                       "iterations": len(rows), **aggregate_summary(total_us, len(rows)),
                       "disk_bytes": backend.disk_bytes(), "checkpoint_us": -1, "recovery_us": -1, "notes": ""})

    samples = []
    sample_count = min(POINT_READ_ITERATIONS, len(rows))
    stride = max(1, len(rows) // sample_count)
    for i in range(0, len(rows), stride):
        if len(samples) >= sample_count:
            break
        start = time.perf_counter()
        found = backend.get(rows[i].id)
        samples.append(elapsed_us(start))
        assert found is not None, "point_read: expected row missing"
    csv_writer.write({"engine": backend.engine_name, "adapter_kind": "cross-language-reference",
                       "workload": "point_read", "durability_profile": profile, "dataset_size": size_name,
                       "iterations": len(samples), **summarize(samples, POINT_READ_WARMUP),
                       "disk_bytes": backend.disk_bytes(), "checkpoint_us": -1, "recovery_us": -1, "notes": ""})

    samples = []
    iterations = range_scan_iterations_for(size_name)
    for i in range(iterations):
        start = time.perf_counter()
        backend.range_scan(i % TAG_CARDINALITY, 50)
        samples.append(elapsed_us(start))
    csv_writer.write({"engine": backend.engine_name, "adapter_kind": "cross-language-reference",
                       "workload": "range_scan", "durability_profile": profile, "dataset_size": size_name,
                       "iterations": len(samples), **summarize(samples, 2), "disk_bytes": backend.disk_bytes(),
                       "checkpoint_us": -1, "recovery_us": -1,
                       "notes": "equality filter + order-by-seq + limit; no secondary index used"})

    samples = []
    for _ in range(CHECKPOINT_ITERATIONS):
        start = time.perf_counter()
        backend.checkpoint()
        samples.append(elapsed_us(start))
    stats = summarize(samples, 0)
    csv_writer.write({"engine": backend.engine_name, "adapter_kind": "cross-language-reference",
                       "workload": "checkpoint", "durability_profile": profile, "dataset_size": size_name,
                       "iterations": len(samples), **stats, "disk_bytes": backend.disk_bytes(),
                       "checkpoint_us": stats["mean_us"], "recovery_us": -1, "notes": ""})

    samples = []
    for _ in range(RECOVERY_ITERATIONS):
        backend.close()
        start = time.perf_counter()
        backend.reopen()
        samples.append(elapsed_us(start))
    stats = summarize(samples, 0)
    csv_writer.write({"engine": backend.engine_name, "adapter_kind": "cross-language-reference",
                       "workload": "recovery", "durability_profile": profile, "dataset_size": size_name,
                       "iterations": len(samples), **stats, "disk_bytes": backend.disk_bytes(),
                       "checkpoint_us": -1, "recovery_us": stats["mean_us"], "notes": ""})

    backend.close()


def run_small_durable_txn(csv_writer: CsvWriter, backend_factory, scratch: Path, profile: str) -> None:
    backend = backend_factory()
    backend.open(fresh_dir(scratch, "small_txn"), profile)
    rows = generate_dataset("small")
    samples = []
    for i in range(min(SMALL_TXN_ITERATIONS, len(rows))):
        start = time.perf_counter()
        backend.put_commit(rows[i])
        samples.append(elapsed_us(start))
    csv_writer.write({"engine": backend.engine_name, "adapter_kind": "cross-language-reference",
                       "workload": "small_durable_txn", "durability_profile": profile, "dataset_size": "n/a",
                       "iterations": len(samples), **summarize(samples, SMALL_TXN_WARMUP),
                       "disk_bytes": backend.disk_bytes(), "checkpoint_us": -1, "recovery_us": -1, "notes": ""})
    backend.close()


def run_bulk_load_per_row_commit(csv_writer: CsvWriter, backend_factory, scratch: Path, profile: str) -> None:
    backend = backend_factory()
    backend.open(fresh_dir(scratch, "bulk_per_row"), profile)
    rows = generate_dataset("small")
    start = time.perf_counter()
    backend.bulk_load(rows, 1)
    total_us = elapsed_us(start)
    csv_writer.write({"engine": backend.engine_name, "adapter_kind": "cross-language-reference",
                       "workload": "bulk_load_per_row_commit", "durability_profile": profile,
                       "dataset_size": "small", "iterations": len(rows), **aggregate_summary(total_us, len(rows)),
                       "disk_bytes": backend.disk_bytes(), "checkpoint_us": -1, "recovery_us": -1,
                       "notes": "one commit per row, contrast with bulk_load's single commit"})
    backend.close()


def run_mixed_read_write(csv_writer: CsvWriter, backend_factory, scratch: Path, profile: str) -> None:
    backend = backend_factory()
    backend.open(fresh_dir(scratch, "mixed"), profile)
    rows = generate_dataset("small")
    backend.bulk_load(rows, len(rows))

    rng = SplitMix64(DATASET_SEED + 1)
    samples = []
    for _ in range(MIXED_ITERATIONS):
        row = rows[rng.next() % len(rows)]
        start = time.perf_counter()
        if rng.next() % 10 < 8:
            backend.get(row.id)
        else:
            updated = Record(row.id, row.tag, rng.next() % 1_000_000, row.payload)
            backend.put_commit(updated)
        samples.append(elapsed_us(start))
    csv_writer.write({"engine": backend.engine_name, "adapter_kind": "cross-language-reference",
                       "workload": "mixed_read_write", "durability_profile": profile, "dataset_size": "small",
                       "iterations": len(samples), **summarize(samples, MIXED_WARMUP),
                       "disk_bytes": backend.disk_bytes(), "checkpoint_us": -1, "recovery_us": -1,
                       "notes": "80/20 read/write mix"})
    backend.close()


def run_engine(csv_writer: CsvWriter, backend_factory, scratch_root: Path) -> None:
    name = backend_factory().engine_name
    for profile in ("durable", "relaxed"):
        profile_scratch = scratch_root / name / profile
        print(f"running {name} / {profile}...", file=sys.stderr)
        run_small_durable_txn(csv_writer, backend_factory, profile_scratch, profile)
        for size_name in ("small", "medium", "large"):
            run_bulk_read_query_lifecycle(csv_writer, backend_factory, profile_scratch, profile, size_name)
        run_bulk_load_per_row_commit(csv_writer, backend_factory, profile_scratch, profile)
        run_mixed_read_write(csv_writer, backend_factory, profile_scratch, profile)


def main() -> int:
    output_csv = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("sqlalchemy_summary.csv")
    # Deliberately not tempfile.mkdtemp()'s default location: /tmp is tmpfs (RAM-backed) on many
    # Linux systems, where fsync is nearly free — see the matching comment in comparative_main.cpp.
    scratch_root = Path.cwd() / "opheap-sqlalchemy-bench-scratch"
    shutil.rmtree(scratch_root, ignore_errors=True)
    scratch_root.mkdir(parents=True)
    try:
        csv_writer = CsvWriter(output_csv)
        run_engine(csv_writer, CoreBackend, scratch_root)
        run_engine(csv_writer, OrmBackend, scratch_root)
        csv_writer.close()
        print(f"csv={output_csv}")
    finally:
        shutil.rmtree(scratch_root, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
