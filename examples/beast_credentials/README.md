# Boost.Beast credentials example (planned)

This directory is reserved for the service-level example described in `docs/credentials-service.md`.

The intended dependency direction is:

```text
Boost.Beast -> credentials domain service -> opheap
```

`opheap` itself will not depend on Boost, OpenSSL, libsodium, Argon2 implementations, HTTP routing, or rate-limiting libraries.

The credentials example should be added only with production-oriented password storage semantics: no plaintext persistence, modern configurable password KDF, constant-time comparison, anti-enumeration behavior and explicit abuse/rate-limit policy.
