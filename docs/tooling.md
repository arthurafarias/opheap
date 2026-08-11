---
layout: default
title: Tooling
---

# Command-line tools

The workspace ships thin process front ends over the core library. Each tool's argument
parsing, encoding and command dispatch live in a header-only module library; the
`applications/*/src/main.cpp` file for each tool only turns `argv`/`stdin`/`stdout` into
calls against that library. Building them is controlled by the top-level
`OPHEAP_BUILD_APPLICATIONS` option (on by default):

```bash
cmake -S . -B build -DOPHEAP_BUILD_APPLICATIONS=ON
cmake --build build
```

## `opheap-cli`

A small tool for creating, reading, updating and deleting JSON roots in an opheap store
from the shell. Implemented by `opheap::module::cli` (`libraries/libopheap-module-cli`),
built on the JSON codec in `libraries/libopheap-utils-serialization-json`.

```text
usage: opheap-cli [-C <heap-directory>] <command> [arguments]

commands:
  create <name> <json|->  create a named JSON root
  get <path>              print a root or dotted object path as JSON
  inspect                 print all named roots as a JSON object
  update <name> <json|-> replace an existing root
  delete <name>           logically delete an existing root
  checkpoint              compact the WAL into a snapshot
  verify                  verify the snapshot and WAL
```

`-C`/`--path` selects the heap directory (defaults to `.`). A JSON argument of `-` reads
the payload from stdin instead of argv, so pipelines work:

```bash
opheap-cli -C service-state create doc '{"name":"Arthur","age":42}'
opheap-cli -C service-state get doc.name
echo '{"name":"Arthur","age":43}' | opheap-cli -C service-state update doc -
opheap-cli -C service-state inspect
opheap-cli -C service-state verify
```

`get` accepts a dotted path (`root.users.arthur.age`) that walks nested objects starting
from a root name. `create`/`update`/`delete` each commit a single transaction.

## `opheap-sql`

An interactive REPL for a minimal SQL dialect executed directly against an opheap-backed
store: `CREATE TABLE`, `INSERT`, `SELECT` with `WHERE`/`ORDER BY`/`LIMIT`, `UPDATE`, and
`DELETE`, with `AND`/`OR`/`NOT` supported in predicates. Implemented by
`opheap::module::sql` (`libraries/libopheap-module-sql`): lexer, parser, AST, interpreter
and REPL loop are all header-only; `applications/opheap-sql` is the process entry point.

```bash
opheap-sql service-state
```

```text
opheap-sql: minimal SQL over an opheap store. Statements end with ';'. .tables lists tables, .exit quits.
sql> CREATE TABLE users (id INT, name TEXT, age INT);
sql> INSERT INTO users VALUES (1, 'Arthur', 42);
sql> SELECT name, age FROM users WHERE age > 18 ORDER BY age LIMIT 10;
sql> .tables
sql> .exit
```

Statements are terminated by `;` and may span multiple input lines. `.tables` lists known
tables and `.exit`/`.quit` leaves the REPL, checkpointing the heap on the way out.

## `opheap-browser`

A placeholder application (`applications/opheap-browser`) for an interactive viewer of
the object tree inside an opheap store. It currently only prints that it is not yet
implemented; no browsing UI exists.
