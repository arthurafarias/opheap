# Changelog

## Unreleased — modular workspace, CLI and SQL front ends

- Split the project into `libraries/` (header-only) and `applications/` (thin process
  front ends) under a CMake super-build.
- Extracted the core engine into `libraries/libopheap-core`.
- Extracted JSON serialization for `opheap::value` into
  `libraries/libopheap-utils-serialization-json`, shared by the CLI and other
  consumers instead of being CLI-only.
- Added `libraries/libopheap-module-cli` and the `opheap-cli` application: JSON root
  create/get/inspect/update/delete, plus `checkpoint` and `verify`, over a heap
  directory.
- Added `libraries/libopheap-module-sql` and the `opheap-sql` application: an
  interactive REPL for a minimal SQL dialect (`CREATE TABLE`, `INSERT`,
  `SELECT`/`WHERE`/`ORDER BY`/`LIMIT`, `UPDATE`, `DELETE`) executed directly against an
  opheap-backed store.
- Scaffolded `applications/opheap-browser` as a placeholder executable; the interactive
  tree viewer is not yet implemented.
- Added `OPHEAP_BUILD_APPLICATIONS` CMake option (on by default).
- Added CLI and SQL (lexer/parser/interpreter/REPL) test coverage and a shared
  deterministic fault-injection test harness.

## 0.2.0 — demand-loaded committed state

- Replaced resident committed root payloads with compact payload locators.
- Snapshot v2 stores a checksummed root index separately from the payload area.
- Opening a checkpointed heap loads metadata only; payloads are fetched on first root access.
- Added bounded LRU payload caching via `heap_config::cache_bytes`.
- Added `heap::cache()` metrics for bytes, entries, hits, misses and evictions.
- WAL recovery now publishes payload offsets rather than retaining all decoded root bytes.
- Checkpoints stream payloads in 64 KiB chunks instead of constructing a complete in-memory snapshot.
- Active transaction working copies remain PMR-owned and therefore naturally pin the state they are using.
- Added lazy-load, cache-hit, eviction and oversized-root regression tests.
- WAL record bytes remain format-v1 compatible; the snapshot file layout is version 2 and pre-1.0 snapshot migration is not provided by this commit.

## 0.1.0 — architectural MVP

- Standalone project detached from CXORM.
- STL-style lowercase public API.
- Observer protocol for persistent mutation.
- `property`, `string`, `vector`, `map`, `value`, `array`, `object`.
- PMR allocation-observing decorator.
- Transaction-local PMR working copies.
- Named Variant roots and dirty-root coalescing.
- Optimistic root-version conflict detection.
- Checksummed WAL with BEGIN/ROOT_UPDATE/COMMIT.
- Strict persistence barriers through platform storage backend.
- Atomic snapshot checkpoints and idempotent recovery.
- Torn-tail handling and corruption detection.
- Self-registering per-class test architecture.
- Deterministic storage-fault tests for partial append, failed barrier and interrupted checkpoint replacement.
- GCC/Clang strict-warning validation and ASan/UBSan support.
- GitHub Pages documentation and project landing page.
- Benchmark harness and future Boost.Beast credentials-service design.
