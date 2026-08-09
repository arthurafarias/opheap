---
layout: default
title: Software Requirements Specification
---

# Software Requirements Specification
## Observable Transactional Persistent Heap for STL-Shaped C++ State

**Project:** opheap  
**Language:** C++23 baseline; C++20-compatible design where practical  
**Deployment:** self-hosted, single process, multiple threads  
**Persistence guarantee:** crash-consistent after each successful strict commit  
**Programming abstraction:** observable persistent C++ properties and STL-shaped containers  
**Canonical data model:** named roots containing Variant object/array trees  
**Storage model:** volatile transaction working sets + cached committed state + append-only WAL + checkpoints

---

## 1. Purpose

The system shall provide a standalone persistence runtime in which service state can be represented directly as C++ object/container structures rather than translated through an object-relational mapping layer.

The system shall make persistent mutation observable through a small interface implemented by persistence-safe properties and STL-shaped container decorators. The active transaction shall receive change events, coalesce them into dirty logical state and persist that state only when the transaction commits.

The system shall treat non-volatile storage as a durable lower tier beneath volatile working memory. The public programming model shall remain independent of the physical storage engine.

## 2. Architectural motivation

The system is motivated by four observations:

1. many service data models are naturally trees or graphs of maps, arrays and values;
2. relational storage frequently introduces a second structural representation of the same state;
3. C++ allocators can control allocation but cannot observe arbitrary writes to already allocated primitive fields;
4. persistence can therefore be made explicit and type-safe by routing mutable persistent state through observable property/container interfaces.

The design shall pursue orthogonal-persistence-like ergonomics without claiming transparent persistence for arbitrary unmodified C++ objects.

## 3. Scope

### 3.1 In scope

The system shall support:

- observable scalar/object properties;
- observable string, sequence and associative containers;
- a universal Variant object model;
- named persistent roots;
- explicit transactions;
- read-your-writes;
- optimistic conflict detection;
- crash-consistent WAL commits;
- recovery and idempotent replay;
- checkpoints;
- integrity checking;
- PMR-backed volatile working sets;
- allocation observation for standard PMR interoperability;
- single-process multi-thread usage;
- unit, integration, corruption, concurrency and recovery testing;
- benchmark-driven optimization.

### 3.2 Out of scope for version 0.1

Version 0.1 shall not require:

- SQL;
- ORM/entity mapping;
- distributed consensus;
- multi-process shared mutation;
- transparent observation of arbitrary raw C++ stores;
- persistent raw virtual addresses;
- automatic persistence of an unmodified `std::vector<int>` solely because it uses a custom allocator;
- password/authentication policy inside the core persistence library.

## 4. Design principles

### 4.1 Persistence through observation

Persistent mutations shall emit change events. Emitting an event shall not itself perform durable I/O. The transaction shall collect/coalesce events and determine what must be persisted at commit.

### 4.2 No uncontrolled mutable primitive state in the automatic model

The persistence-safe typed model shall prefer `property<T>` for scalar/member values and bindable observable types for container elements/mapped values.

An API shall not return a mutable `T&` from `property<T>::get()` because that would allow writes that bypass observation.

### 4.3 STL-shaped naming and behavior

Public types and functions shall use lowercase STL-style naming. Containers shall support conventional iterator/range usage and familiar operations where those operations can preserve observer correctness.

### 4.4 Composition over STL modification

The project shall not fork or patch the C++ standard library. Observable containers shall be implemented as decorators/compositions over standard or PMR containers.

### 4.5 Allocation observation is not write observation

The system shall expose a `std::pmr::memory_resource` decorator for allocation/deallocation events, while documentation and APIs shall clearly state that allocator hooks cannot detect arbitrary mutations to allocated bytes.

## 5. Public object model

### 5.1 Observer interface

The library shall expose a `change_observer` interface receiving `change_event` records.

A change event shall include at minimum:

- logical root token;
- change kind;
- optional offset hint;
- optional byte-count hint.

Change kinds shall include:

- value mutation;
- structural mutation;
- allocation;
- deallocation.

### 5.2 Observable base

An `observable` base/mixin shall hold an observer binding and provide protected notification support.

Binding an observable container shall recursively bind its observable children.

### 5.3 `property<T>`

`property<T>` shall:

- reject raw pointer/reference types;
- support const reads;
- support observable assignment;
- avoid notifications for equal assignments where equality is available;
- support controlled complex mutation through a callback;
- provide arithmetic convenience operators where valid;
- preserve the destination observer when assigned/moved from external values.

### 5.4 `string`

The persistent string shall use PMR storage and expose familiar immutable reads plus observable mutating operations.

### 5.5 `vector<T>`

The persistent vector shall:

- use PMR allocation;
- require observable/bindable element types;
- expose familiar iteration and indexed access;
- emit structural events for insertion/erasure/capacity changes;
- preserve/rebind child observers when reallocation moves elements.

### 5.6 `map<K,V>`

The persistent map shall:

- use PMR allocation;
- require observable/bindable mapped values;
- permit conventional key types;
- emit structural events for insertion/erasure/clear;
- bind newly created mapped values before returning them to application mutation.

### 5.7 Universal Variant tree

The library shall provide a universal `value` with the alternatives:

- null;
- boolean;
- signed 64-bit integer;
- double;
- persistent string;
- persistent array;
- persistent object/map.

Mutable object/array access may lazily convert the current node to the requested container type and shall emit a structural mutation. Const typed access shall reject a mismatched type.

The library shall support JavaScript-like chained object construction:

```cpp
root["users"]["42"]["active"] = true;
```

## 6. Memory-resource model

Transaction working state shall be allocated from a PMR resource owned by that transaction.

The library shall provide an `observing_resource` which decorates another `std::pmr::memory_resource` and reports allocation/deallocation events.

The observing resource shall not claim to provide mutation observation for ordinary standard containers.

Future allocator implementations may provide size classes, arenas, NUMA placement or persistent extents without changing the observer contract.

## 7. Transaction model

### 7.1 Operations

Transactions shall support:

- begin;
- open/read named root;
- mutate root through observable state;
- commit;
- abort.

### 7.2 Working copies

A transaction shall materialize each named root on first access into transaction-owned volatile memory.

Uncommitted mutation shall not modify the committed global root cache.

### 7.3 Dirty tracking

The transaction shall implement `change_observer`. Observer events shall mark the associated root dirty.

Multiple writes to the same root shall be coalesced so the root is serialized at most once per commit.

### 7.4 Read-your-writes

All reads through the same transaction shall immediately observe previous writes performed by that transaction.

### 7.5 Abort

Abort shall discard the transaction working state without durable rollback records because uncommitted versions shall not have been published.

## 8. Concurrency

### 8.1 Process model

Version 0.1 shall support multiple threads in one process.

### 8.2 Optimistic versioning

Every named root shall have a monotonically increasing committed version.

A transaction shall record the expected version observed when it materializes the root.

At commit, every dirty root's expected version shall be compared with the currently committed version.

If any dirty root version differs, commit shall fail with `conflict_error` and no part of the transaction shall become visible.

### 8.3 Commit ordering

Version 0.1 may serialize final WAL publication through a commit mutex because one ordered journal is used. Mutation and transaction preparation shall remain independent of that lock.

Future versions should reduce commit serialization through batching/group commit or partitioned logs while maintaining total recovery semantics.

## 9. Durable storage model

The durable store shall contain at minimum:

- snapshot/checkpoint file;
- append-only journal file.

The committed in-process cache shall be reconstructible from these durable files.

## 10. Journal requirements

### 10.1 Record types

The WAL shall include at minimum:

- BEGIN;
- ROOT_UPDATE;
- COMMIT.

Future formats may add allocation, free, extent, reference, abort or checkpoint records without changing the public observer model.

### 10.2 Record framing

Each record shall contain:

- magic value;
- format version;
- record type;
- monotonically ordered sequence number;
- transaction ID;
- payload length;
- checksum when enabled.

### 10.3 Root update

A root update shall contain:

- name;
- expected version;
- new version;
- stable type identifier;
- serialized payload.

### 10.4 Commit

COMMIT shall include enough information to verify that the complete expected write set was present, including at least the number of root updates.

## 11. Commit protocol

For a transaction with dirty state, the system shall:

1. serialize each dirty root;
2. enter commit publication order;
3. validate all expected root versions;
4. append BEGIN;
5. append all ROOT_UPDATE records;
6. append COMMIT;
7. execute the configured durability barrier;
8. publish new committed root versions to the volatile cache;
9. release commit ordering;
10. return success.

A strict commit shall not return success before the persistence barrier succeeds.

If a persistence operation fails after journal mutation may have begun, the live heap shall enter a poisoned/fail-stop state rather than accept further commits on uncertain durability state.

## 12. Recovery requirements

Startup shall:

- load/validate the checkpoint if present;
- scan the journal sequentially;
- validate record framing and checksums;
- identify complete and incomplete transactions;
- apply only complete committed transactions;
- preserve transaction atomicity;
- ignore an incomplete final transaction;
- detect a partial final record as a torn tail;
- truncate the torn tail to the last valid byte when opening for normal operation;
- reject checksum corruption in a complete record;
- reconstruct committed root versions;
- make repeated recovery idempotent.

## 13. Checkpoint requirements

Checkpoint shall:

1. capture a consistent committed-root image and current journal sequence;
2. write a temporary snapshot;
3. synchronize snapshot contents in strict mode;
4. atomically replace the old snapshot;
5. synchronize the directory entry where supported;
6. truncate the journal;
7. synchronize the truncation in strict mode.

If a crash leaves both the new snapshot and old WAL, recovery shall skip root updates whose versions are already contained in the snapshot.

## 14. Integrity requirements

The library shall expose an integrity check returning a report rather than requiring application parsing of storage files.

Integrity checking shall validate:

- snapshot checksum and framing;
- WAL framing;
- WAL checksums;
- transaction begin/commit structure;
- update-count consistency;
- root version chains;
- format versions.

## 15. Serialization requirements

The persistence format shall use explicit byte serialization rather than persisting compiler-specific STL object layouts or process virtual addresses.

The canonical Variant type shall have stable tagged encoding.

The type tag written for a durable root shall not depend on `typeid(T).name()` or other compiler-unstable identifiers.

Future typed-model codecs shall require explicitly stable schema/type identifiers.

## 16. Crash consistency

In strict durability mode:

- after successful commit return, recovery shall reproduce the corresponding logical root versions even after immediate power/process failure;
- if failure occurs before the commit barrier succeeds, the transaction may be absent;
- partially written final records shall not cause partially visible application state;
- committed-record corruption shall not be silently converted into a truncation event.

## 17. Portability

Durable file operations shall be isolated behind `storage_file` and `storage_backend` interfaces.

The default implementation shall use:

- POSIX `pread`/`pwrite`, `fdatasync`/`fsync`, `rename`, directory `fsync`; or
- Windows random-access file I/O, `FlushFileBuffers`, and write-through replacement primitives.

## 18. Configuration

At minimum, configuration shall include:

- storage path;
- checkpoint journal-size threshold;
- durability mode;
- checksum enablement.

Strict crash-consistent durability and checksums shall be enabled by default.

## 19. Performance requirements

The architecture shall optimize for:

- in-memory read latency;
- low mutation overhead (observer event rather than I/O);
- coalesced write sets;
- low durable commit latency;
- bounded recovery time through checkpointing;
- future lazy materialization/eviction;
- future granularity finer than a complete root.

Performance work shall not weaken documented durability semantics.

## 20. Benchmark requirements

Benchmarks shall include:

- hot point lookup;
- scalar/object mutation;
- small transaction;
- larger tree transaction;
- independent-root transactions;
- same-root contention;
- checkpoint latency;
- recovery latency;
- cold/warm/hot cache behavior once bounded caching exists.

External comparisons shall record hardware, OS, compiler, storage, configuration, cache state, transaction size and durability mode.

Durable commits shall only be compared with competitors configured for equivalent durability semantics.

## 21. Testing requirements

Every public class and durable subsystem shall have a self-registering test group in a sibling `*_test.hpp` file.

The aggregate test build shall include all test headers in one generated translation unit.

The suite shall cover:

- unit behavior;
- observer propagation;
- container reallocation;
- codec round trips;
- integration persistence;
- restart recovery;
- conflicts;
- truncation;
- corruption;
- checkpoints;
- sanitizer builds;
- concurrency.

Before 1.0, deterministic storage fault injection shall additionally cover failed/partial writes, failed barriers and interruption at checkpoint boundaries.

## 22. Documentation requirements

The repository shall contain:

- architectural-motivation README;
- complete SRS;
- architecture document;
- programming-model guide;
- durability/recovery specification;
- API guide;
- testing guide;
- benchmarking guide;
- roadmap;
- GitHub Pages project site;
- documented integration architecture for a future Boost.Beast credentials REST service.

## 23. Credentials-service integration requirements

The core persistence library shall remain independent of Boost and authentication libraries.

A future Boost.Beast credentials service shall:

- store only password-verification material, never plaintext passwords;
- use a modern password KDF with explicit parameters;
- use constant-time verification where applicable;
- avoid user-enumeration response differences;
- support rate limiting/abuse controls outside the persistence layer;
- benchmark persistence overhead separately from intentionally expensive KDF cost;
- preserve strict commit durability for credential changes where configured.

## 24. Acceptance criteria for the MVP

The MVP is acceptable when:

- the library builds under the documented C++ baseline with strict warnings;
- Variant object trees can be created and mutated through observable APIs;
- mutations dirty their transaction root automatically;
- commits survive close/reopen;
- aborted mutations do not appear;
- same-root optimistic conflicts are deterministic;
- independent roots can commit without logical conflict;
- incomplete WAL tails are discarded;
- complete corrupted WAL records are rejected;
- checkpoints recover correctly and replay is idempotent;
- PMR allocation observation works;
- each implemented class/subsystem has a self-registering test group;
- the suite passes under ASan/UBSan on a supported build;
- GitHub Pages documentation is publishable from the repository.

## 25. Future acceptance criteria for 1.0

Version 1.0 additionally requires:

- deterministic barrier/write fault injection across the full commit/checkpoint state machine;
- bounded cache with safe pin/eviction semantics;
- finer-grained versioning than whole-root replacement for large datasets;
- stable format compatibility/migration policy;
- benchmark adapters and reproducible external comparisons;
- performance regression gates;
- service-level example demonstrating production-oriented integration.

## 26. Summary

`opheap` shall provide a persistent programming model in which application state remains an observable C++ object/container graph. The durable backend shall behave as a lower storage tier reached through transaction commit, rather than requiring application data to be remodeled into tables.

The fundamental C++ compromise is explicit: raw writes are not observable, so automatic persistence is restricted to types whose mutation surface participates in the observer protocol. That restriction provides a clean correctness boundary and makes persistence behavior testable rather than implicit.
