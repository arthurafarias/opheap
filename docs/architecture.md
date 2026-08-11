---
layout: default
title: Architecture
---

# Architecture

## One logical state, multiple physical tiers

`opheap` treats durable media as a persistence tier beneath a volatile working set.

```text
Application model
     │
     ▼
Observable STL-shaped object graph
     │
     ▼
Transaction-local PMR working set
     │       mutation events
     └──────────────┐
                    ▼
              dirty-root set
                    │
                 commit
                    ▼
        optimistic version validation
                    │
                    ▼
             append-only WAL
                    │
             persistence barrier
                    │
                    ▼
        root metadata + locator index
                    │
          ┌─────────┴─────────┐
          ▼                   ▼
 bounded payload cache   snapshot / WAL
          │                   │
          └──── demand load ──┘
                    │
                checkpoint
                    │
                    ▼
       indexed snapshot + payload area
```

## Components

| Component | Responsibility |
|---|---|
| `change_observer` | Receives logical mutation events. |
| `observable` | Stores the observer binding and emits events. |
| `property<T>` | Makes scalar/object replacement observable. |
| `string` | Observable PMR string. |
| `vector<T>` | Observable STL-shaped sequence; children must be bindable. |
| `map<K,V>` | Observable associative container; mapped values must be bindable. |
| `value` | Universal Variant node. |
| `array` / `object` | Variant sequence/map containers. |
| `observing_resource` | PMR allocator decorator for allocation/deallocation events. |
| `transaction` | Observer, working-set owner, dirty-set owner, OCC participant. |
| `journal` | Ordered crash-consistent transaction log. |
| `payload_cache` | Bounded LRU of encoded committed root payloads; active readers pin through shared ownership. |
| `snapshot_store` | Indexed checkpoint image; metadata is loaded eagerly, payloads are read by locator. |
| `heap` | Lifecycle, recovery, commit ordering and integrity API. |

## Why root-level versioning first

The current implementation versions named logical roots rather than individual fields or heap pages. This gives a simple correctness model:

- transaction working copies are isolated;
- observer events collapse into one dirty bit per root;
- conflict detection compares one expected version per dirty root;
- recovery installs whole-root replacement records.

It is deliberately an MVP granularity, not a permanent limitation. The observer protocol already carries a change kind plus offset/size hints, so the storage engine can later version independent extents without requiring a new application model.

## Allocation observation vs write observation

`std::pmr::memory_resource` is an allocation interface. It provides an elegant hook for placement and allocation policy, but it is not a write barrier. Therefore `observing_resource` is useful for topology metrics and custom allocation, while observable wrappers remain responsible for logical mutation tracking.

## Historical motivation

The architecture is conceptually related to:

- orthogonal persistence: object longevity should not force a different programming model;
- persistence by reachability: durable state is naturally rooted in a persistent graph;
- single-level storage: main memory and secondary storage can be understood as one storage system with different physical placement;
- persistent allocators such as Metall: C++ data structures can be built directly over persistence-oriented memory allocators.

`opheap` combines those ideas with explicit transactions and a portable WAL rather than requiring byte-addressable persistent memory hardware.


## Repository layout

The workspace is a CMake super-build over separately consumable pieces rather than one
monolithic target:

| Path | Role |
|---|---|
| `libraries/libopheap-core` | The header-only persistence engine described above (`opheap::opheap`). |
| `libraries/libopheap-utils-serialization-json` | Header-only JSON parser/writer for `opheap::value` (`opheap::json`), independent of any one front end. |
| `libraries/libopheap-module-cli` | Header-only argument parsing and command dispatch for `opheap-cli` (`opheap::module::cli`). |
| `libraries/libopheap-module-sql` | Header-only lexer, parser, AST, interpreter and REPL for `opheap-sql`'s SQL dialect (`opheap::module::sql`). |
| `applications/opheap-cli` | Process entry point over `opheap::module::cli`: a JSON root CRUD tool. |
| `applications/opheap-sql` | Process entry point over `opheap::module::sql`: an interactive SQL REPL against an opheap store. |
| `applications/opheap-browser` | Placeholder process for an interactive object-tree viewer; not yet implemented. |

Each module library is independently linkable — `libopheap-module-sql` does not depend on
`libopheap-module-cli` or vice versa — and each application is a thin `argv`/`stdin`/`stdout`
adapter with no logic beyond wiring. See [Tooling](tooling.md) for command references.

## Demand-loaded committed state

`heap_state` no longer owns a `name -> payload` map. It owns `name -> {version, type, loc}` metadata. A transaction first probes the bounded payload cache; on a miss the locator is read from either the snapshot or WAL and the encoded root is decoded into that transaction's PMR pool. Checkpoints stream payloads in fixed-size chunks, so checkpoint memory use is bounded independently of durable dataset size.
