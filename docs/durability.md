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
8. publish the new root records in the volatile cache;
9. return success.

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

1. load the latest valid snapshot, if present;
2. scan the WAL sequentially;
3. accumulate transactions beginning with `BEGIN`;
4. collect root replacement records;
5. apply only transactions ending in a valid `COMMIT` with the expected update count;
6. ignore incomplete transactions;
7. skip WAL root versions already represented by the snapshot;
8. truncate a partial tail.

Because WAL updates are full logical root replacements tagged with versions, replay is idempotent.

## Checkpoint protocol

Checkpoint writes a full snapshot to a temporary file, syncs it, atomically replaces the previous snapshot, syncs the directory where supported, then truncates and syncs the WAL.

If a crash occurs after snapshot replacement but before journal truncation, the old WAL can still exist. Recovery skips records whose root version is already present in the snapshot, making that interruption safe.
