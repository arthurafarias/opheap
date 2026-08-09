# Changelog

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
