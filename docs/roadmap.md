---
layout: default
title: Roadmap
---

# Roadmap

## 0.1 — implemented MVP

- observable property/container model;
- Variant map/array application state;
- transaction-local PMR working sets;
- root-level optimistic conflicts;
- durable WAL;
- snapshots/checkpoints;
- recovery and integrity checking;
- per-class self-registering tests;
- sanitizer configuration;
- GitHub Pages documentation.

## 0.2 — demand-loaded committed state

- locator-only resident root index;
- bounded LRU cache for encoded committed payloads;
- lazy root payload materialization;
- chunked checkpoint copy independent of total dataset size;
- cache accounting through `heap::cache()`;
- workspace split into `libraries/` (core, JSON serialization, CLI and SQL modules) and `applications/` (thin process front ends);
- `opheap-cli`: JSON root create/get/inspect/update/delete/checkpoint/verify over a heap directory;
- `opheap-sql`: an interactive REPL for a minimal SQL dialect (`CREATE TABLE`, `INSERT`, `SELECT`/`WHERE`/`ORDER BY`/`LIMIT`, `UPDATE`, `DELETE`) against an opheap store;
- `opheap-browser` scaffolded as a placeholder executable; the interactive tree viewer itself is not yet implemented.

## 0.3 — granularity

- independently versioned extents below a logical root;
- dirty path/range coalescing;
- lazy materialization of cold branches within a large root;
- extent pinning/handles;
- allocator size classes.

## 0.4 — performance

- journal group commit;
- batched checksums;
- faster CRC implementation selected at runtime;
- concurrent commit preparation;
- recovery index/checkpoint acceleration;
- benchmark adapters for SQLite/LMDB/RocksDB.

## 0.5 — typed models

- public codec/schema extension mechanism;
- reflection-assisted member registration when compiler support is practical;
- stable schema identifiers and migration hooks;
- optional persistent references for non-tree graphs.

## 0.6 — service examples

- Boost.Beast credentials service;
- durable session/index example;
- latency and recovery comparison under strict durability.

## 1.0

Requires a complete crash-fault matrix, stable format-compatibility policy, extent-level granularity, reproducible benchmark matrix and explicit upgrade/migration tooling.
