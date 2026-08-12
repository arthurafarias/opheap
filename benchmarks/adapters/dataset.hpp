#pragma once

// Shared, deterministic dataset generator for the comparative benchmark suite.
//
// Every adapter (native C++ and, per benchmarks/reference/*, the Python/Rust reference
// scripts) replays the exact same operation sequence so that measured differences reflect
// the storage engine, not the input. The generator is a splitmix64 PRNG (not std::mt19937,
// whose implementation is not guaranteed identical across languages/standard libraries) so
// Python and Rust reimplementations can reproduce the same size distribution byte-for-byte
// from the same seed. The formula is documented in docs/benchmarks.md.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace opheap_bench {

struct splitmix64 {
    std::uint64_t state;
    explicit splitmix64(std::uint64_t seed) : state(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    // Uniform integer in [lo, hi], inclusive.
    std::uint64_t range(std::uint64_t lo, std::uint64_t hi) { return lo + next() % (hi - lo + 1); }
};

struct record {
    std::int64_t id{};
    std::int32_t tag{};
    std::int32_t seq{};
    std::string payload;
};

struct size_class {
    std::string_view name;
    std::size_t row_count;
    std::size_t min_payload_bytes;
    std::size_t max_payload_bytes;
};

// tuned relative to the 2 MiB cache_bytes override the comparative harness uses (see
// comparative_main.cpp): "small" comfortably fits, "medium" exceeds it, "large" stresses
// checkpoint/compaction cost further.
inline constexpr size_class dataset_small{"small", 1'000, 32, 128};
inline constexpr size_class dataset_medium{"medium", 10'000, 128, 512};
inline constexpr size_class dataset_large{"large", 20'000, 256, 1024};

inline constexpr std::uint64_t dataset_seed = 0xC0FFEEULL;
inline constexpr std::int32_t tag_cardinality = 10;

inline std::vector<record> generate_dataset(const size_class& size, std::uint64_t seed = dataset_seed) {
    splitmix64 rng{seed};
    std::vector<record> rows;
    rows.reserve(size.row_count);
    for (std::size_t i = 0; i < size.row_count; ++i) {
        record r;
        r.id = static_cast<std::int64_t>(i);
        r.tag = static_cast<std::int32_t>(i % static_cast<std::size_t>(tag_cardinality));
        r.seq = static_cast<std::int32_t>(size.row_count - i);
        const auto len = rng.range(size.min_payload_bytes, size.max_payload_bytes);
        r.payload.resize(len);
        for (auto& c : r.payload) c = static_cast<char>('a' + rng.next() % 26);
        rows.push_back(std::move(r));
    }
    return rows;
}

// Fixed-width, zero-padded decimal so lexicographic key order (LMDB/RocksDB cursors, opheap
// root_names()) matches numeric id order.
inline std::string encode_key(std::int64_t id) {
    std::string key(20, '0');
    for (std::size_t i = key.size(); i-- > 0 && id > 0; id /= 10) key[i] = static_cast<char>('0' + id % 10);
    return key;
}

// Minimal fixed-layout binary encoding for backends with no native schema (LMDB/RocksDB):
// int64 id | int32 tag | int32 seq | remaining bytes = payload.
inline std::string encode_record(const record& r) {
    std::string bytes(16, '\0');
    std::memcpy(bytes.data(), &r.id, 8);
    std::memcpy(bytes.data() + 8, &r.tag, 4);
    std::memcpy(bytes.data() + 12, &r.seq, 4);
    bytes += r.payload;
    return bytes;
}

inline record decode_record(std::string_view bytes) {
    record r;
    std::memcpy(&r.id, bytes.data(), 8);
    std::memcpy(&r.tag, bytes.data() + 8, 4);
    std::memcpy(&r.seq, bytes.data() + 12, 4);
    r.payload.assign(bytes.substr(16));
    return r;
}

} // namespace opheap_bench
