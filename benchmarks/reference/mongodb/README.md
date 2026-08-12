# MongoDB reference (not implemented yet)

Deferred: exercising this would require pulling and running a local `mongod` (e.g. `docker run
-d -p 27017:27017 mongo`), which is a standing service, not a one-shot local build — starting it
is left to whoever runs this, not to repo automation. MongoDB is otherwise the natural NoSQL
document-store peer to compare against opheap's JSON-like `opheap::value` model (nested
maps/arrays, no schema), closer in spirit than the embedded KV stores (LMDB/RocksDB) in Tier A.

## Intended shape, matching the other Tier B scripts

- `pymongo`, single `records` collection, one document per row:
  `{"_id": id, "tag": tag, "seq": seq, "payload": payload}`.
- Same seeded dataset generator as `../python_sqlalchemy/bench.py` (splitmix64 port of
  `../../adapters/dataset.hpp`).
- Durability profile mapping: `durable` → `write_concern=WriteConcern(w=1, j=True)` (journaled
  acknowledged write); `relaxed` → `write_concern=WriteConcern(w=1, j=False)`.
- Workloads: same set as the other Tier B scripts (`small_durable_txn`, `bulk_load`,
  `bulk_load_per_row_commit`, `point_read`, `range_scan`, `checkpoint`, `recovery`,
  `mixed_read_write`), with:
  - `range_scan` → `collection.find({"tag": tag}).sort("seq", 1).limit(limit)` (no secondary
    index created, same "lowest common denominator query" rule as every other adapter in this
    suite — see `docs/benchmarks.md`);
  - `checkpoint` → no direct equivalent; closest analog is `db.command("fsync")`, worth noting
    in `notes` as approximate;
  - `recovery` → close the client and reconnect; MongoDB's own crash recovery (WiredTiger
    checkpoints + journal replay) is a server-side concern this client-side timing wouldn't
    actually exercise, so that caveat belongs in the `notes` column too.
- Same CSV schema and `adapter_kind=cross-language-reference` tag as every other Tier B script.

Whoever picks this up: follow `../python_sqlalchemy/bench.py`'s structure (`Record` dataclass,
`SplitMix64`, `CsvWriter`, per-workload functions) — the shape is designed to be copied.
