---
layout: default
title: opheap
description: Observable transactional persistence for STL-shaped C++ state.
---

<div class="hero">
<h1>Make storage feel like memory again.</h1>
<p><strong>opheap</strong> is a C++23 transactional persistent heap built around observable STL-style objects. Model service state as a Variant tree, mutate it normally, then commit the changed logical roots to durable storage.</p>
</div>

```cpp
opheap::heap state = opheap::heap::open({.path = "state"});

auto tx = state.begin();
auto& root = tx.object_root();

root["users"]["arthur"]["age"] = 42;
root["users"]["arthur"]["roles"].as_array().push_back(opheap::value{"admin"});

tx.commit();
```

<div class="cards">
<div class="card"><h3>No ORM</h3><p>The object tree is the logical model. There is no mandatory table/entity translation layer.</p></div>
<div class="card"><h3>Observable mutation</h3><p>Properties and STL-shaped containers notify the active transaction instead of issuing I/O per write.</p></div>
<div class="card"><h3>Crash-consistent</h3><p>Checksummed WAL records, durability barriers, checkpoints and idempotent recovery define the persistence boundary.</p></div>
<div class="card"><h3>PMR-aware</h3><p>The runtime uses polymorphic memory resources and exposes an allocation-observing resource for ordinary PMR interoperability.</p></div>
</div>

## Architectural thesis

Conventional service stacks frequently maintain both an in-memory object graph and a database representation of the same logical state. `opheap` explores a different architecture: **DRAM is the fast working tier; non-volatile storage is the durable lower tier; commit is the operation that propagates a logical memory version downward.**

The design draws from orthogonal persistence and single-level storage, while adapting those ideas to C++'s actual type and allocator rules.

## Why wrappers exist

A C++ allocator can see allocation and deallocation, but it cannot intercept an arbitrary assignment to an already allocated `int`. Rather than claim otherwise, `opheap` disallows uncontrolled mutable primitive state inside its automatic model. Mutation passes through `property`, `string`, `vector`, `map`, or the universal `value` tree, each of which participates in the observer protocol.

## Start here

- [Architecture](architecture.md)
- [Programming model](programming-model.md)
- [Durability and recovery](durability.md)
- [API reference](api.md)
- [Testing strategy](testing.md)
- [Benchmarking](benchmarks.md)
- [Boost.Beast credentials-service integration](credentials-service.md)
- [Software Requirements Specification](SRS.md)
- [Roadmap](roadmap.md)
- [References](references.md)
- [Validation record](validation.md)
