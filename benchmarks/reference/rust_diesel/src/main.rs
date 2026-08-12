//! Tier B cross-language reference: Diesel over SQLite.
//!
//! Reference-only — see benchmarks/reference/README.md and docs/benchmarks.md. Every row this
//! binary writes is tagged adapter_kind=cross-language-reference: it runs in a separate
//! process/runtime from the Tier A C++ adapters, so it is not a fair, apples-to-apples
//! comparison against them.
//!
//! The dataset generator is a Rust port of the splitmix64 generator in
//! benchmarks/adapters/dataset.hpp (same seed, same algorithm, same size classes).

use diesel::prelude::*;
use diesel::sql_query;
use std::fs;
use std::path::{Path, PathBuf};
use std::time::Instant;

const DATASET_SEED: u64 = 0xC0FFEE;
const TAG_CARDINALITY: i32 = 10;

struct SizeClass {
    name: &'static str,
    row_count: usize,
    min_bytes: u64,
    max_bytes: u64,
}

const DATASET_SMALL: SizeClass = SizeClass { name: "small", row_count: 1_000, min_bytes: 32, max_bytes: 128 };
const DATASET_MEDIUM: SizeClass = SizeClass { name: "medium", row_count: 10_000, min_bytes: 128, max_bytes: 512 };
const DATASET_LARGE: SizeClass = SizeClass { name: "large", row_count: 20_000, min_bytes: 256, max_bytes: 1024 };

struct SplitMix64 {
    state: u64,
}

impl SplitMix64 {
    fn new(seed: u64) -> Self {
        Self { state: seed }
    }
    fn next(&mut self) -> u64 {
        self.state = self.state.wrapping_add(0x9E3779B97F4A7C15);
        let mut z = self.state;
        z = (z ^ (z >> 30)).wrapping_mul(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)).wrapping_mul(0x94D049BB133111EB);
        z ^ (z >> 31)
    }
    fn range(&mut self, lo: u64, hi: u64) -> u64 {
        lo + self.next() % (hi - lo + 1)
    }
}

diesel::table! {
    records (id) {
        id -> BigInt,
        tag -> Integer,
        seq -> Integer,
        payload -> Text,
    }
}

#[derive(Queryable, Insertable, AsChangeset, Clone)]
#[diesel(table_name = records)]
struct Record {
    id: i64,
    tag: i32,
    seq: i32,
    payload: String,
}

fn generate_dataset(size: &SizeClass, seed: u64) -> Vec<Record> {
    let mut rng = SplitMix64::new(seed);
    let mut rows = Vec::with_capacity(size.row_count);
    for i in 0..size.row_count {
        let len = rng.range(size.min_bytes, size.max_bytes) as usize;
        let payload: String = (0..len).map(|_| (b'a' + (rng.next() % 26) as u8) as char).collect();
        rows.push(Record {
            id: i as i64,
            tag: (i % TAG_CARDINALITY as usize) as i32,
            seq: (size.row_count - i) as i32,
            payload,
        });
    }
    rows
}

struct Summary {
    mean_us: f64,
    p50_us: f64,
    p95_us: f64,
    p99_us: f64,
    throughput_per_s: f64,
}

fn percentile(values: &mut [f64], p: f64) -> f64 {
    if values.is_empty() {
        return 0.0;
    }
    values.sort_by(|a, b| a.partial_cmp(b).unwrap());
    let index = (p * (values.len() - 1) as f64) as usize;
    values[index]
}

fn summarize(samples: &[f64], warmup: usize) -> Summary {
    let mut trimmed: Vec<f64> = if samples.len() > warmup { samples[warmup..].to_vec() } else { samples.to_vec() };
    let mean = if trimmed.is_empty() { 0.0 } else { trimmed.iter().sum::<f64>() / trimmed.len() as f64 };
    Summary {
        mean_us: mean,
        p50_us: percentile(&mut trimmed.clone(), 0.50),
        p95_us: percentile(&mut trimmed.clone(), 0.95),
        p99_us: percentile(&mut trimmed, 0.99),
        throughput_per_s: if mean == 0.0 { 0.0 } else { 1_000_000.0 / mean },
    }
}

fn aggregate_summary(total_us: f64, ops: usize) -> Summary {
    let mean = if ops == 0 { 0.0 } else { total_us / ops as f64 };
    Summary {
        mean_us: mean,
        p50_us: mean,
        p95_us: mean,
        p99_us: mean,
        throughput_per_s: if mean == 0.0 { 0.0 } else { 1_000_000.0 / mean },
    }
}

struct CsvRow<'a> {
    workload: &'a str,
    profile: &'a str,
    dataset_size: &'a str,
    iterations: usize,
    stats: Summary,
    disk_bytes: u64,
    checkpoint_us: f64,
    recovery_us: f64,
    notes: &'a str,
}

fn write_csv_row(out: &mut String, row: &CsvRow) {
    out.push_str(&format!(
        "diesel,cross-language-reference,{},{},{},{},{},{},{},{},{},{},{},{},\"{}\"\n",
        row.workload, row.profile, row.dataset_size, row.iterations, row.stats.mean_us, row.stats.p50_us,
        row.stats.p95_us, row.stats.p99_us, row.stats.throughput_per_s, row.disk_bytes, row.checkpoint_us,
        row.recovery_us, row.notes
    ));
}

fn elapsed_us(start: Instant) -> f64 {
    start.elapsed().as_secs_f64() * 1_000_000.0
}

fn disk_bytes(path: &Path) -> u64 {
    let mut total = 0u64;
    if let Ok(entries) = fs::read_dir(path) {
        for entry in entries.flatten() {
            if let Ok(metadata) = entry.metadata() {
                if metadata.is_file() {
                    total += metadata.len();
                } else if metadata.is_dir() {
                    total += disk_bytes(&entry.path());
                }
            }
        }
    }
    total
}

fn fresh_dir(root: &Path, label: &str) -> PathBuf {
    let dir = root.join(label);
    let _ = fs::remove_dir_all(&dir);
    fs::create_dir_all(&dir).unwrap();
    dir
}

struct Backend {
    conn: Option<SqliteConnection>,
    path: PathBuf,
    profile: &'static str,
}

impl Backend {
    fn open(path: PathBuf, profile: &'static str) -> Self {
        let mut backend = Backend { conn: None, path, profile };
        backend.connect();
        backend
    }

    fn connect(&mut self) {
        let db_path = self.path.join("records.sqlite3");
        let mut conn = SqliteConnection::establish(db_path.to_str().unwrap()).expect("connect");
        sql_query("PRAGMA journal_mode=WAL").execute(&mut conn).unwrap();
        let synchronous = if self.profile == "durable" { "FULL" } else { "OFF" };
        sql_query(format!("PRAGMA synchronous={synchronous}")).execute(&mut conn).unwrap();
        sql_query(
            "CREATE TABLE IF NOT EXISTS records (\
                id BIGINT PRIMARY KEY, tag INTEGER NOT NULL, seq INTEGER NOT NULL, payload TEXT NOT NULL)",
        )
        .execute(&mut conn)
        .unwrap();
        self.conn = Some(conn);
    }

    fn conn(&mut self) -> &mut SqliteConnection {
        self.conn.as_mut().unwrap()
    }

    fn put_commit(&mut self, r: &Record) {
        diesel::insert_into(records::table)
            .values(r)
            .on_conflict(records::id)
            .do_update()
            .set(r)
            .execute(self.conn())
            .unwrap();
    }

    // SQLite's diesel backend has no multi-row on_conflict batch insert, so each chunk becomes
    // one explicit transaction of single-row inserts rather than one multi-row VALUES statement.
    fn bulk_load(&mut self, rows: &[Record], commit_every: usize) {
        for chunk in rows.chunks(commit_every.max(1)) {
            self.conn()
                .transaction::<_, diesel::result::Error, _>(|conn| {
                    for r in chunk {
                        diesel::insert_into(records::table)
                            .values(r)
                            .on_conflict(records::id)
                            .do_nothing()
                            .execute(conn)?;
                    }
                    Ok(())
                })
                .unwrap();
        }
    }

    fn get(&mut self, id: i64) -> Option<Record> {
        records::table.find(id).first::<Record>(self.conn()).optional().unwrap()
    }

    fn range_scan(&mut self, tag: i32, limit: i64) -> Vec<Record> {
        records::table
            .filter(records::tag.eq(tag))
            .order(records::seq.asc())
            .limit(limit)
            .load::<Record>(self.conn())
            .unwrap()
    }

    fn checkpoint(&mut self) {
        sql_query("PRAGMA wal_checkpoint(TRUNCATE)").execute(self.conn()).unwrap();
    }

    fn close(&mut self) {
        self.conn = None;
    }

    fn reopen(&mut self) {
        self.connect();
    }

    fn disk_bytes(&self) -> u64 {
        disk_bytes(&self.path)
    }
}

const SMALL_TXN_ITERATIONS: usize = 200;
const SMALL_TXN_WARMUP: usize = 20;
const POINT_READ_ITERATIONS: usize = 300;
const POINT_READ_WARMUP: usize = 20;
const CHECKPOINT_ITERATIONS: usize = 3;
const RECOVERY_ITERATIONS: usize = 3;
const MIXED_ITERATIONS: usize = 200;
const MIXED_WARMUP: usize = 20;

fn range_scan_iterations_for(size: &SizeClass) -> usize {
    if size.row_count <= DATASET_SMALL.row_count {
        20
    } else if size.row_count <= DATASET_MEDIUM.row_count {
        10
    } else {
        4
    }
}

fn run_bulk_read_query_lifecycle(out: &mut String, scratch: &Path, profile: &'static str, size: &SizeClass) {
    let mut backend = Backend::open(fresh_dir(scratch, size.name), profile);
    let rows = generate_dataset(size, DATASET_SEED);

    let start = Instant::now();
    backend.bulk_load(&rows, rows.len());
    let total_us = elapsed_us(start);
    write_csv_row(
        out,
        &CsvRow {
            workload: "bulk_load", profile, dataset_size: size.name, iterations: rows.len(),
            stats: aggregate_summary(total_us, rows.len()), disk_bytes: backend.disk_bytes(),
            checkpoint_us: -1.0, recovery_us: -1.0, notes: "",
        },
    );

    let mut samples = Vec::new();
    let sample_count = POINT_READ_ITERATIONS.min(rows.len());
    let stride = (rows.len() / sample_count).max(1);
    let mut i = 0;
    while i < rows.len() && samples.len() < sample_count {
        let start = Instant::now();
        let found = backend.get(rows[i].id);
        samples.push(elapsed_us(start));
        assert!(found.is_some(), "point_read: expected row missing");
        i += stride;
    }
    write_csv_row(
        out,
        &CsvRow {
            workload: "point_read", profile, dataset_size: size.name, iterations: samples.len(),
            stats: summarize(&samples, POINT_READ_WARMUP), disk_bytes: backend.disk_bytes(),
            checkpoint_us: -1.0, recovery_us: -1.0, notes: "",
        },
    );

    let mut samples = Vec::new();
    let iterations = range_scan_iterations_for(size);
    for i in 0..iterations {
        let start = Instant::now();
        let _ = backend.range_scan((i % TAG_CARDINALITY as usize) as i32, 50);
        samples.push(elapsed_us(start));
    }
    write_csv_row(
        out,
        &CsvRow {
            workload: "range_scan", profile, dataset_size: size.name, iterations: samples.len(),
            stats: summarize(&samples, 2), disk_bytes: backend.disk_bytes(), checkpoint_us: -1.0,
            recovery_us: -1.0, notes: "equality filter + order-by-seq + limit; no secondary index used",
        },
    );

    let mut samples = Vec::new();
    for _ in 0..CHECKPOINT_ITERATIONS {
        let start = Instant::now();
        backend.checkpoint();
        samples.push(elapsed_us(start));
    }
    let stats = summarize(&samples, 0);
    let checkpoint_us = stats.mean_us;
    write_csv_row(
        out,
        &CsvRow {
            workload: "checkpoint", profile, dataset_size: size.name, iterations: samples.len(), stats,
            disk_bytes: backend.disk_bytes(), checkpoint_us, recovery_us: -1.0, notes: "",
        },
    );

    let mut samples = Vec::new();
    for _ in 0..RECOVERY_ITERATIONS {
        backend.close();
        let start = Instant::now();
        backend.reopen();
        samples.push(elapsed_us(start));
    }
    let stats = summarize(&samples, 0);
    let recovery_us = stats.mean_us;
    write_csv_row(
        out,
        &CsvRow {
            workload: "recovery", profile, dataset_size: size.name, iterations: samples.len(), stats,
            disk_bytes: backend.disk_bytes(), checkpoint_us: -1.0, recovery_us, notes: "",
        },
    );

    backend.close();
}

fn run_small_durable_txn(out: &mut String, scratch: &Path, profile: &'static str) {
    let mut backend = Backend::open(fresh_dir(scratch, "small_txn"), profile);
    let rows = generate_dataset(&DATASET_SMALL, DATASET_SEED);
    let mut samples = Vec::new();
    for r in rows.iter().take(SMALL_TXN_ITERATIONS) {
        let start = Instant::now();
        backend.put_commit(r);
        samples.push(elapsed_us(start));
    }
    write_csv_row(
        out,
        &CsvRow {
            workload: "small_durable_txn", profile, dataset_size: "n/a", iterations: samples.len(),
            stats: summarize(&samples, SMALL_TXN_WARMUP), disk_bytes: backend.disk_bytes(),
            checkpoint_us: -1.0, recovery_us: -1.0, notes: "",
        },
    );
    backend.close();
}

fn run_bulk_load_per_row_commit(out: &mut String, scratch: &Path, profile: &'static str) {
    let mut backend = Backend::open(fresh_dir(scratch, "bulk_per_row"), profile);
    let rows = generate_dataset(&DATASET_SMALL, DATASET_SEED);
    let start = Instant::now();
    backend.bulk_load(&rows, 1);
    let total_us = elapsed_us(start);
    write_csv_row(
        out,
        &CsvRow {
            workload: "bulk_load_per_row_commit", profile, dataset_size: "small", iterations: rows.len(),
            stats: aggregate_summary(total_us, rows.len()), disk_bytes: backend.disk_bytes(),
            checkpoint_us: -1.0, recovery_us: -1.0,
            notes: "one commit per row, contrast with bulk_load's single commit",
        },
    );
    backend.close();
}

fn run_mixed_read_write(out: &mut String, scratch: &Path, profile: &'static str) {
    let mut backend = Backend::open(fresh_dir(scratch, "mixed"), profile);
    let rows = generate_dataset(&DATASET_SMALL, DATASET_SEED);
    backend.bulk_load(&rows, rows.len());

    let mut rng = SplitMix64::new(DATASET_SEED + 1);
    let mut samples = Vec::new();
    for _ in 0..MIXED_ITERATIONS {
        let row = &rows[(rng.next() as usize) % rows.len()];
        let start = Instant::now();
        if rng.next() % 10 < 8 {
            let _ = backend.get(row.id);
        } else {
            let updated = Record { id: row.id, tag: row.tag, seq: (rng.next() % 1_000_000) as i32, payload: row.payload.clone() };
            backend.put_commit(&updated);
        }
        samples.push(elapsed_us(start));
    }
    write_csv_row(
        out,
        &CsvRow {
            workload: "mixed_read_write", profile, dataset_size: "small", iterations: samples.len(),
            stats: summarize(&samples, MIXED_WARMUP), disk_bytes: backend.disk_bytes(), checkpoint_us: -1.0,
            recovery_us: -1.0, notes: "80/20 read/write mix",
        },
    );
    backend.close();
}

fn main() {
    let output_path = std::env::args().nth(1).unwrap_or_else(|| "diesel_summary.csv".to_string());
    // Deliberately not std::env::temp_dir(): /tmp is tmpfs (RAM-backed) on many Linux systems,
    // where fsync is nearly free — see the matching comment in comparative_main.cpp.
    let scratch_root = std::env::current_dir().unwrap().join("opheap-diesel-bench-scratch");
    let _ = fs::remove_dir_all(&scratch_root);
    fs::create_dir_all(&scratch_root).unwrap();

    let mut out = String::from(
        "engine,adapter_kind,workload,durability_profile,dataset_size,iterations,mean_us,p50_us,p95_us,p99_us,\
         throughput_per_s,disk_bytes,checkpoint_us,recovery_us,notes\n",
    );

    for profile in ["durable", "relaxed"] {
        let profile_scratch = scratch_root.join(profile);
        eprintln!("running diesel / {profile}...");
        run_small_durable_txn(&mut out, &profile_scratch, profile);
        for size in [&DATASET_SMALL, &DATASET_MEDIUM, &DATASET_LARGE] {
            run_bulk_read_query_lifecycle(&mut out, &profile_scratch, profile, size);
        }
        run_bulk_load_per_row_commit(&mut out, &profile_scratch, profile);
        run_mixed_read_write(&mut out, &profile_scratch, profile);
    }

    fs::write(&output_path, out).unwrap();
    let _ = fs::remove_dir_all(&scratch_root);
    println!("csv={output_path}");
}
