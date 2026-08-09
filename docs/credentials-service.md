---
layout: default
title: Boost.Beast Credentials Service
---

# Future Boost.Beast credentials REST service

A credentials service is a useful stress test for `opheap`: lookups should be memory-resident when hot, updates should become short durable transactions, and restart should reconstruct the same logical credential index without ORM hydration.

## Layering

```text
Boost.Beast HTTP
      │
      ▼
credential_service
      │
      ├── validation / rate limits / audit policy
      ├── password KDF / constant-time verification
      │
      ▼
opheap transaction
      │
      ▼
credential Variant tree
      │
      ▼
WAL + checkpoint
```

The HTTP layer must not be linked into the `opheap` library itself.

## Proposed logical model

```text
root["credentials"][principal_id] = {
    "scheme": "argon2id",
    "salt": <encoded bytes>,
    "digest": <encoded hash>,
    "parameters": {...},
    "credential_version": N,
    "disabled": false,
    "created_at": ...,
    "updated_at": ...
}
```

Plaintext credentials must never be persisted.

## Proposed endpoints

- `POST /v1/credentials` — provision/replace a credential under authenticated administrative policy.
- `POST /v1/credentials/verify` — verify a presented secret without revealing whether failure was caused by a missing principal or a bad password.
- `DELETE /v1/credentials/{principal}` — disable/remove under policy.
- `GET /healthz` — service health only; no credential data.

## Performance rule

“Ultra fast” must mean low storage/index overhead **while retaining an intentionally expensive password KDF**. Benchmarking must not weaken Argon2id/scrypt/PBKDF2 parameters, remove rate limiting, or create user-enumeration differences merely to improve requests per second.

## Concurrency

A credential record can be partitioned into one named root per shard or principal range. That is a natural future benchmark for the planned finer-grained root/extent architecture.
