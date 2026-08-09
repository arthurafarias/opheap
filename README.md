# opheap

**Observable, transactional persistence for STL-shaped C++ state.**

`opheap` is an experiment in treating durable storage as a lower level of the memory hierarchy instead of forcing application state through a relational model.

The programming goal is intentionally ordinary:

```cpp
opheap::heap state = opheap::heap::open({.path = "service-state"});

{
    auto tx = state.begin();
    auto& root = tx.object_root();

    root["users"]["arthur"]["age"] = 42;
    root["users"]["arthur"]["name"] = "Arthur";
    root["users"]["arthur"]["roles"].as_array().push_back(opheap::value{"admin"});

    tx.commit();
}
```

No table mapping is involved. The tree in memory is the application model, and commit makes a new logical version of that tree durable.

## Why this exists

Most service code has two representations of the same state:

1. an object graph used by the program; and
2. a storage model used because the durable medium is treated as a database rather than another memory tier.

That split creates serializers, entities, repositories, table mappings, schema adapters and object-relational impedance. `opheap` asks a different question:

> What if an application could model state directly with C++ objects and STL-shaped containers, while a transactional memory service handled durability underneath them?

The idea is related to **orthogonal persistence**—uniform treatment of objects regardless of longevity—and to **single-level storage**, where main memory and secondary storage are presented as one conceptual storage system. It also follows the allocator direction already present in modern C++ through `std::pmr::memory_resource`.

## The C++ problem: writes are not observable

A C++ allocator can observe this:

```cpp
allocate(...);
deallocate(...);
```

but it cannot observe this:

```cpp
user.age = 42;
vector[3] = value;
```

if those writes mutate already allocated bytes.

`opheap` does **not** hide that limitation. It solves it by making persistent mutation explicit in the type system:

```cpp
opheap::property<int> age;
opheap::string name;
opheap::vector<opheap::value> items;
opheap::map<std::string, opheap::value> properties;
```

All mutable persistent leaves and containers implement the observer protocol. A transaction is the observer. Mutations mark a logical root dirty; they do not perform I/O immediately.

```text
property/container mutation
          │
          ▼
   change_observer
          │
          ▼
 transaction dirty set
          │
          │ commit()
          ▼
 version validation
          │
          ▼
 append-only WAL
          │
          ▼
 persistence barrier
          │
          ▼
 publish new root version
```

Thousands of in-memory mutations can therefore collapse into one durable root update.

## STL interoperability without pretending STL is observable

`opheap` has two levels of interoperability.

### 1. Allocation interoperability

`opheap::observing_resource` is a `std::pmr::memory_resource` decorator and can be passed to ordinary PMR containers:

```cpp
opheap::observing_resource resource{std::pmr::get_default_resource(), binding};
std::pmr::vector<int> values{&resource};
```

It reports allocation and deallocation topology. It **cannot** report arbitrary writes to `values[i]` because the C++ allocator contract has no such hook.

### 2. Persistence-safe containers

For automatic mutation tracking, use the STL-shaped observable layer:

- `opheap::property<T>`
- `opheap::string`
- `opheap::vector<T>`
- `opheap::map<K, V>`
- `opheap::value`
- `opheap::array`
- `opheap::object`

The universal data model is `opheap::value`, a JavaScript-like Variant tree:

```text
null | bool | int64 | double | string | array | object
```

That lets a service express its entire logical state as nested maps/arrays without introducing an ORM.

## Persistence model

Version 0.1 uses:

- transaction-local PMR working sets;
- named Variant roots;
- observer-driven dirty-root tracking;
- optimistic root-version conflict detection;
- an append-only checksummed write-ahead journal;
- a persistence barrier before successful commit returns;
- atomic snapshot replacement for checkpoints;
- version-aware WAL replay so a pre-checkpoint journal can safely survive an interrupted checkpoint;
- truncated-tail detection and committed-record checksum validation.

The first version intentionally persists at **logical-root granularity**. The public observer interface is designed so later implementations can split roots into objects, extents, cache lines, or pages without changing application code.

## Crash semantics

With the default `durability_mode::strict`:

- if `commit()` returns successfully, the corresponding root versions have crossed the backend persistence barrier;
- a transaction without a complete valid COMMIT record is ignored during recovery;
- a partial final WAL record is treated as a torn tail and truncated to the last valid record;
- checksum corruption in a complete record is reported as corruption, not silently ignored;
- checkpoint replay is idempotent.

## Concurrency

Transactions hold independent working copies. Mutations require no global application lock. Commits are serialized only across the short validation/WAL publication path because version 0.1 uses one ordered journal.

If two transactions modify the same root from the same version:

```text
T1 reads root v7
T2 reads root v7
T1 commits -> v8
T2 commits -> conflict_error
```

Independent roots can commit from concurrent snapshots without a logical conflict.

## Build

Requirements:

- CMake 3.24+
- C++23 compiler (the code is intentionally C++20-compatible in most places, but the project baseline is C++23)
- POSIX or Windows durable-file primitives

```bash
cmake -S . -B build \
  -DOPHEAP_BUILD_TESTS=ON \
  -DOPHEAP_BUILD_EXAMPLES=ON

cmake --build build
ctest --test-dir build --output-on-failure
```

Sanitizers:

```bash
cmake -S . -B build-asan \
  -DOPHEAP_BUILD_TESTS=ON \
  -DOPHEAP_ENABLE_SANITIZERS=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

## Testing architecture

Every class or durable subsystem has a sibling self-registering test header such as:

```text
tests/opheap/testing/property_test.hpp
tests/opheap/testing/vector_test.hpp
tests/opheap/testing/journal_test.hpp
tests/opheap/testing/heap_test.hpp
```

Each file defines an `inline static test_group`. CMake discovers all `*_test.hpp` files and generates one aggregate translation unit. This deliberately catches header-level namespace collisions and ODR mistakes that independent test executables can hide.

Current coverage includes:

- observer binding and change events;
- scalar properties and controlled mutation;
- observable strings, vectors and maps;
- observer rebinding after vector reallocation;
- universal Variant trees;
- codec round trips;
- PMR allocation observation;
- durable storage operations;
- WAL replay and torn tails;
- snapshot replacement;
- restart persistence;
- abort semantics;
- optimistic conflicts;
- checkpoint/recovery idempotence;
- checksum corruption rejection;
- deterministic partial-append / failed-barrier / interrupted-checkpoint fault injection;
- independent-root transactions;
- actual multi-threaded independent-root commits;
- ASan and UBSan builds.

## Documentation

The complete Software Requirements Specification and design documentation are under [`docs/`](docs/index.md):

- Architecture
- Programming model
- Durability protocol
- API
- Testing
- Benchmarking
- Future Boost.Beast credentials REST service
- Roadmap
- Full SRS

The repository includes a GitHub Pages workflow and a small Jekyll-compatible project site.

## Future Boost.Beast credentials service

A planned example uses `opheap` behind a Boost.Beast REST service for credentials. Persistence and HTTP remain separate layers. The design explicitly does **not** trade password-hash cost, rate limiting, constant-time verification, or enumeration resistance for benchmark numbers. “Ultra fast” applies to state lookup/commit overhead under equivalent security parameters, not to weakening the password KDF.

See [`docs/credentials-service.md`](docs/credentials-service.md).

## What opheap is not

`opheap` is not:

- a SQL database;
- an ORM;
- a distributed database;
- a transparent persistence mechanism for arbitrary unmodified C++ objects;
- a claim that passing a custom allocator to `std::vector<int>` makes element writes observable;
- a replacement for authentication hardening or cryptographic password storage.

## Project status

Version `0.1.0` is a testable architectural MVP. The durability protocol, observer model, Variant tree, STL-shaped containers, checkpoint/recovery path and optimistic conflicts are implemented. The most important next performance step is to split large logical roots into independently versioned persistent extents while preserving this API.

## Design lineage and references

- Dearle, Kirby, Morrison, *Orthogonal Persistence Revisited*.
- IBM i documentation on single-level storage.
- C++ `std::pmr::memory_resource` and `std::pmr::polymorphic_allocator`.
- Iwabuchi et al., *Metall: A Persistent Memory Allocator For Data-Centric Analytics*.

These are architectural precedents, not claims that `opheap` duplicates any one of those systems.

## License

No license is imposed by this generated prototype. Add the license you want before publishing or accepting external contributions.
