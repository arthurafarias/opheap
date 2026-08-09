---
layout: default
title: Programming Model
---

# Programming model

## Universal service state

The canonical model is a named `value` root:

```text
value := null | bool | int64 | double | string | array | object
array := vector<value>
object := map<string,value>
```

This is intentionally close to a JavaScript object model while remaining statically implemented in C++.

```cpp
auto tx = heap.begin();
auto& app = tx.object_root("application");

app["accounts"]["42"]["email"] = "a@example.test";
app["accounts"]["42"]["active"] = true;
app["accounts"]["42"]["roles"].as_array().push_back(opheap::value{"admin"});

tx.commit();
```

## Observable leaves

For typed models, raw mutable primitives should be wrapped:

```cpp
struct account {
    opheap::property<std::uint64_t> login_count;
    opheap::string display_name;
};
```

`property<T>::get()` returns a const reference. Complex mutation is performed through `mutate`, preventing a mutable reference from escaping observation:

```cpp
settings.mutate([](auto& value) {
    value.normalize();
});
```

## Containers

`vector<T>` requires a bindable element type so that `operator[]` cannot silently expose an unobservable mutable scalar. Use `property<int>` rather than `int` where element-level assignment must be persistent.

```cpp
opheap::vector<opheap::property<int>> counters;
```

`map<K,V>` applies the same rule to mapped values. Keys are immutable under the normal `std::map` contract and may use ordinary value types.

## Transactions

A transaction owns a PMR pool and decoded working copies of roots requested by the application. A root is decoded only on first access.

```cpp
auto tx = heap.begin();
auto& root = tx.root("metrics");
```

Read-your-writes is immediate because application code modifies the transaction's working object directly. `commit()` publishes all dirty roots atomically in one WAL transaction.

## Abort

Destroying an uncommitted transaction or calling `abort()` discards the working copies. No rollback writes are needed because uncommitted copies were never published.
