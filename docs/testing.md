---
layout: default
title: Testing
---

# Testing

## Per-class self-registering groups

Each header-only library carries its own tests, colocated with the classes they cover, under
that library's `include/.../testing/` tree — the testing facility ships in the source code of
the library itself rather than living in a separate top-level tree:

```text
libraries/libopheap-core/include/opheap/testing/observer_test.hpp
libraries/libopheap-core/include/opheap/testing/property_test.hpp
libraries/libopheap-core/include/opheap/testing/string_test.hpp
libraries/libopheap-core/include/opheap/testing/vector_test.hpp
libraries/libopheap-core/include/opheap/testing/map_test.hpp
libraries/libopheap-core/include/opheap/testing/value_test.hpp
libraries/libopheap-core/include/opheap/testing/memory_resource_test.hpp
libraries/libopheap-core/include/opheap/testing/codec_test.hpp
libraries/libopheap-core/include/opheap/testing/storage_test.hpp
libraries/libopheap-core/include/opheap/testing/journal_test.hpp
libraries/libopheap-core/include/opheap/testing/snapshot_test.hpp
libraries/libopheap-core/include/opheap/testing/transaction_test.hpp
libraries/libopheap-core/include/opheap/testing/heap_test.hpp
libraries/libopheap-core/include/opheap/testing/fault_injection_test.hpp
libraries/libopheap-module-cli/include/opheap/module/cli/testing/cli_test.hpp
libraries/libopheap-module-sql/include/opheap/module/sql/testing/sql_lexer_test.hpp
libraries/libopheap-module-sql/include/opheap/module/sql/testing/sql_parser_test.hpp
libraries/libopheap-module-sql/include/opheap/module/sql/testing/sql_interpreter_test.hpp
libraries/libopheap-module-sql/include/opheap/module/sql/testing/sql_repl_test.hpp
```

The shared harness (`test_group`, `test_case`, `test_context`, `test_failure`, `registry`,
`run_all`, plus fixtures like `temporary_directory`/`recording_observer`) lives alongside them
at `libraries/libopheap-core/include/opheap/testing/`, since `libopheap-core` is already a
transitive dependency of every other library.

Each test header declares a dedicated `opheap::testing` struct named after the class it covers,
deriving from `test_group`:

```cpp
// libraries/libopheap-core/include/opheap/testing/heap_test.hpp
namespace opheap::testing {

struct heap_test : public test_group {
    heap_test() : test_group("heap", {
        {"persistent object tree survives restart", [](test_context& ctx) {
            // ...
        }},
    }) {}
};

inline static heap_test heap_test_instance;

} // namespace opheap::testing
```

Materialization is automatic, exactly as before: constructing `heap_test_instance` at static-init
time runs `test_group`'s constructor, which self-registers the group into `registry()`. CMake
globs every library's `include/.../testing/*_test.hpp` and generates one source file containing
all the includes. This mirrors the useful property of CXORM's newer test architecture: every test
suite coexists in one translation unit, exposing accidental helper/name collisions and ODR
problems.

Because tests live under `include/`, each library's `install(DIRECTORY include/ ...)` excludes the
`testing/` subdirectory, so installed packages ship production headers only.

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
