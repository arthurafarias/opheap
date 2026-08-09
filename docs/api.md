---
layout: default
title: API
---

# API

## `heap`

```cpp
static heap open(heap_config,
                 std::shared_ptr<storage_backend> = make_default_storage_backend());
transaction begin();
void checkpoint();
integrity_report check_integrity() const noexcept;
std::size_t root_count() const noexcept;
void close() noexcept;
```

## `transaction`

```cpp
value& root(std::string_view name = "root");
object& object_root(std::string_view name = "root");
void commit();
void abort() noexcept;
bool active() const noexcept;
transaction_id id() const noexcept;
std::size_t dirty_roots() const noexcept;
```

`transaction` also implements `change_observer`; persistent wrappers bind directly to it.

## `property<T>`

```cpp
const T& get() const noexcept;
property& operator=(const T&);
property& operator=(T&&);
template<class F> decltype(auto) mutate(F&&);
```

Arithmetic convenience operators are provided when supported by `T`.

## `string`

STL-shaped operations include assignment, `append`, `push_back`, `erase`, `clear`, `size`, `empty`, `c_str`, and `view`.

## `vector<T>`

Requires a bindable element type. Provides STL-shaped iteration, `operator[]`, `at`, `emplace_back`, `push_back`, `erase`, `clear`, `reserve`, `size`, and `empty`.

## `map<K,V>`

Requires a bindable mapped type. Provides `operator[]`, `try_emplace`, `erase`, `clear`, `find`, `at`, `contains`, iteration, `size`, and `empty`.

## `value`

Variant alternatives:

```text
null_t
bool
int64_t
double
opheap::string
shared array
shared object
```

Mutable `as_array()` and `as_object()` lazily convert the value to the requested container type and report structural mutation. Const accessors are strict and throw `type_error` on mismatches.

## `observing_resource`

A PMR `memory_resource` decorator that reports allocation and deallocation events to a `change_observer`. It is not a write barrier and therefore does not make unmodified STL element mutation persistent by itself.
