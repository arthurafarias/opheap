---
layout: default
title: 0.2 Commit Plan
---

# Commit plan — demand-loaded committed state

## Proposed commit

```text
feat(storage): demand-load committed roots through bounded cache
```

## Intent

Remove the invariant that every committed root payload is resident after `heap::open()`. Keep only root metadata and durable locators in memory; fetch a root payload only when a transaction accesses it. Preserve the existing observer, transaction, OCC and public STL-shaped programming model.

## Code changes

1. **Locator model** — `detail::root_record` stores `{version, type, loc}` rather than an owned payload vector.
2. **WAL locators** — journal append/recovery returns offsets and lengths for committed payload bytes already present in the WAL.
3. **Indexed snapshot v2** — snapshot startup reads a compact checksummed index. Payloads remain in the file and are addressed by offsets.
4. **Bounded cache** — add an internal LRU keyed by root name/version and configured by `heap_config::cache_bytes`.
5. **Demand loading** — `transaction::root()` resolves metadata, probes the cache, and reads/decodes only the requested payload on a miss.
6. **Checkpoint streaming** — copy payloads from current locators to the replacement snapshot in bounded chunks; never build the complete durable image in RAM.
7. **Locator lifetime safety** — serialize cache-miss storage reads against checkpoint/WAL reclamation and re-read metadata after acquiring commit order.
8. **Observability** — expose `heap::cache()` for cache bytes, entries, hits, misses and evictions.

## Explicit non-goals for this commit

- No bare-metal physical-address backend.
- No filesystem removal from the desktop/backend implementation.
- No change to the observer-based mutation API.
- No object/field-level extents yet; the demand-load unit remains one named root.
- No raw durable virtual pointers.
- No SQL/ORM layer.

## Compatibility

WAL framing is unchanged and remains record format 1. Snapshot layout becomes format 2 because metadata and payload bytes are separated. This is a pre-1.0 storage-format break; automatic v1 snapshot migration is deferred until the project defines a stable migration policy.

## Acceptance tests

- Existing durability, recovery, conflict, corruption and fault-injection tests pass unchanged in behavior.
- Immediately after checkpoint/reopen, `heap::cache().bytes == 0` even when roots exist.
- First root access produces a miss and materializes only that root.
- Repeated access to the same version produces a cache hit.
- Accessing additional roots evicts old entries while resident cache bytes stay within `cache_bytes`.
- A root larger than the cache limit remains usable but is never retained in the shared cache.
- Checkpoint/recovery remains idempotent.
- Strict-warning GCC/Clang and ASan/UBSan builds pass before merge.

## Follow-up commit

The next storage-granularity commit should replace whole-root payload locators with independently addressable extents/pages. That will let `root["users"][id]` fetch only the relevant branch rather than decoding the complete root while keeping the public API unchanged.
