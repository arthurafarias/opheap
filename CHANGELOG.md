# Changelog

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
