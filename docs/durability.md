---
layout: default
title: Durability
---

# Durability and recovery

## Commit protocol

For every dirty root, a transaction captures:

- root name;
- expected version;
- new version;
- stable type tag;
- serialized Variant payload.

The commit path is:

1. serialize dirty roots;
2. acquire commit publication order;
3. compare current root versions with expected versions;
4. append `BEGIN`;
5. append one `ROOT_UPDATE` record per dirty root;
6. append `COMMIT` with the update count;
7. execute the configured persistence barrier;
8. publish the new root versions and durable payload locators;
9. retain the encoded payload in the bounded cache when it fits;
10. return success.

The default strict backend uses `fdatasync`/`fsync` on POSIX and `FlushFileBuffers` on Windows.

## WAL framing

Every record includes:

- magic;
- record format;
- record type;
- monotonic sequence number;
- transaction ID;
- payload length;
- CRC32C.

A partial final frame is a torn tail. Recovery truncates it to the last complete record. A complete frame with an invalid checksum is corruption and causes open/integrity checking to fail.

## Recovery

Startup performs:

1. load only the latest snapshot header and root locator index, if present;
2. scan the WAL sequentially;
3. accumulate transactions beginning with `BEGIN`;
4. collect root metadata and WAL payload locators without retaining every payload;
5. apply only transactions ending in a valid `COMMIT` with the expected update count;
6. ignore incomplete transactions;
7. skip WAL root versions already represented by the snapshot;
8. truncate a partial tail.

Because WAL updates are full logical root replacements tagged with versions, replay is idempotent.

## Checkpoint protocol

Checkpoint writes a compact checksummed root index followed by the root payload area to a temporary snapshot. Payloads are copied from their current snapshot/WAL locators in bounded chunks rather than assembled into one in-memory image. The snapshot is synced, atomically installed, the directory is synced where supported, and only then is the WAL truncated and synced.

If a crash occurs after snapshot replacement but before journal truncation, the old WAL can still exist. Recovery skips records whose root version is already present in the snapshot, making that interruption safe.
