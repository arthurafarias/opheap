# Contributing

`opheap` treats persistence correctness as part of the API contract. Changes to hot paths are welcome, but performance changes must not weaken crash semantics, observer propagation or deterministic conflict detection.

## Before submitting a change

```bash
cmake -S . -B build -DOPHEAP_BUILD_TESTS=ON -DOPHEAP_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure

cmake -S . -B build-asan -DOPHEAP_BUILD_TESTS=ON -DOPHEAP_ENABLE_SANITIZERS=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

## Test architecture

Add or update a sibling `tests/opheap/testing/*_test.hpp` group for every class/subsystem changed. Tests self-register as `inline static test_group` objects and are compiled together in one generated translation unit.

## Durable format changes

Any change to WAL or snapshot framing must:

1. change or explicitly preserve the on-disk format version;
2. document compatibility behavior;
3. add recovery tests for old/new combinations when compatibility is promised;
4. include corruption/truncation cases;
5. update `docs/durability.md` and `docs/SRS.md`.

## Performance changes

Include before/after benchmark evidence and state durability mode, hardware and compiler. Do not compare strict durable commits against asynchronous competitors as equivalent modes.
