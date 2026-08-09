---
layout: default
title: Benchmarks
---

# Benchmarking

The performance target is evidence-based: optimize persistent-object workloads under comparable durability semantics rather than claiming that a durable commit is faster than another engine running asynchronously.

The included microbenchmark measures:

- small durable transaction latency;
- hot-root lookup/mutation;
- independent-root throughput;
- checkpoint latency;
- close/reopen recovery latency.

Build with:

```bash
cmake -S . -B build -DOPHEAP_BUILD_BENCHMARKS=ON
cmake --build build
./build/benchmarks/opheap_benchmark
```

Before external performance claims, comparative adapters should record CPU, topology, RAM, storage, kernel, compiler, engine version, dataset, thread count, transaction size, cache state and durability mode. Candidate engines include SQLite, LMDB, RocksDB, LevelDB and persistent-memory-oriented allocators where the environments are comparable.
