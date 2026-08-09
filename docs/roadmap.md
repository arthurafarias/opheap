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

## 0.2 — granularity

- independently versioned extents below a logical root;
- dirty path/range coalescing;
- lazy materialization of cold branches;
- cache accounting and eviction/pinning handles;
- allocator size classes.

## 0.3 — performance

- journal group commit;
- batched checksums;
- faster CRC implementation selected at runtime;
- concurrent commit preparation;
- recovery index/checkpoint acceleration;
- benchmark adapters for SQLite/LMDB/RocksDB.

## 0.4 — typed models

- public codec/schema extension mechanism;
- reflection-assisted member registration when compiler support is practical;
- stable schema identifiers and migration hooks;
- optional persistent references for non-tree graphs.

## 0.5 — service examples

- Boost.Beast credentials service;
- durable session/index example;
- latency and recovery comparison under strict durability.

## 1.0

Requires a complete crash-fault matrix, format-compatibility policy, bounded-cache implementation, reproducible benchmark matrix and explicit upgrade/migration tooling.
