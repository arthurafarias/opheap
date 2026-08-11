---
layout: default
title: Testing
---

# Testing

## Per-class self-registering groups

Tests follow a deliberate one-component/one-header pattern:

```text
tests/opheap/testing/observer_test.hpp
tests/opheap/testing/property_test.hpp
tests/opheap/testing/string_test.hpp
tests/opheap/testing/vector_test.hpp
tests/opheap/testing/map_test.hpp
tests/opheap/testing/value_test.hpp
tests/opheap/testing/memory_resource_test.hpp
tests/opheap/testing/codec_test.hpp
tests/opheap/testing/storage_test.hpp
tests/opheap/testing/journal_test.hpp
tests/opheap/testing/snapshot_test.hpp
tests/opheap/testing/transaction_test.hpp
tests/opheap/testing/heap_test.hpp
tests/opheap/testing/cli_test.hpp
tests/opheap/testing/sql_lexer_test.hpp
tests/opheap/testing/sql_parser_test.hpp
tests/opheap/testing/sql_interpreter_test.hpp
tests/opheap/testing/sql_repl_test.hpp
tests/opheap/testing/fault_injection_test.hpp
```

Each header declares an `inline static test_group`. CMake discovers the headers and generates one source file containing all includes. This mirrors the useful property of CXORM's newer test architecture: every test suite coexists in one translation unit, exposing accidental helper/name collisions and ODR problems.

The naming convention itself is STL-style lowercase/snake_case.

## Correctness classes

The current suite exercises:

- event routing;
- equality coalescing;
- controlled complex mutation;
- container structural notifications;
- rebinding child observers after vector reallocation;
- Variant coercion and strict const reads;
- PMR allocation/deallocation observation;
- codec round trips;
- durable file primitives;
- committed WAL replay;
- torn-tail truncation;
- snapshot round trips;
- read-your-writes;
- abort;
- OCC conflicts;
- restart recovery;
- checkpoint recovery and idempotence;
- checksum corruption rejection;
- independent root commits;
- `opheap-cli` command dispatch (create/get/inspect/update/delete/checkpoint/verify) end to end against a real heap directory;
- SQL lexing, parsing, interpretation and REPL behavior for `opheap-sql`'s dialect;
- deterministic storage fault injection through the shared `fault_backend`/`fault_file`/`fault_plan` harness.

## Sanitizers

`OPHEAP_ENABLE_SANITIZERS=ON` enables AddressSanitizer and UndefinedBehaviorSanitizer on supported non-MSVC builds.

## Future fault matrix

Before 1.0, storage fault injection should systematically cover every barrier boundary:

- partial `write_exact`;
- partial append;
- failed data flush;
- failed full flush;
- failed atomic replacement;
- process termination after each journal record;
- checkpoint termination before/after rename and before/after WAL truncation.
