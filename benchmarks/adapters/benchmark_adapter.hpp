#pragma once

// Common shape every Tier A (in-process C++) comparative adapter implements, plus the shared
// timing/CSV-reporting utilities the driver (comparative_main.cpp) uses against all of them.
// See docs/benchmarks.md for the methodology this encodes.

#include "dataset.hpp"

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opheap_bench {

enum class durability_profile { durable, relaxed };

inline std::string_view to_string(durability_profile p) {
    return p == durability_profile::durable ? "durable" : "relaxed";
}

enum class adapter_kind { embedded_native, embedded_orm, cross_language_reference };

inline std::string_view to_string(adapter_kind k) {
    switch (k) {
        case adapter_kind::embedded_native: return "embedded-native";
        case adapter_kind::embedded_orm: return "embedded-orm";
        case adapter_kind::cross_language_reference: return "cross-language-reference";
    }
    return "unknown";
}

// A predicate over the deterministic `tag` field (see dataset.hpp) — every adapter's
// range_scan matches rows with record.tag == tag, orders by seq ascending, and returns at
// most `limit` rows. This is deliberately the lowest common denominator query every engine in
// the suite can express (equality filter + order + limit), so it doesn't favor engines with a
// secondary-index/query planner over ones without one.
struct tag_predicate {
    std::int32_t tag;
    std::size_t limit;
};

// Structural interface every Tier A adapter satisfies. Concept-checked rather than virtual so
// the driver incurs no dispatch overhead that would skew the very latencies being measured —
// consistent with this codebase's existing use of `requires`-constrained templates (see
// opheap::observable_element in libopheap-core/include/opheap/vector.hpp).
template<class T>
concept backend = requires(T& b, const T& cb, const std::filesystem::path& path,
                            durability_profile profile, std::size_t cache_bytes,
                            const record& rec, std::int64_t id, const std::vector<record>& batch,
                            std::size_t commit_every, tag_predicate pred) {
    { T::engine_name() } -> std::convertible_to<std::string_view>;
    { T::kind() } -> std::same_as<adapter_kind>;
    { b.open(path, profile, cache_bytes) } -> std::same_as<void>;
    { b.put_commit(rec) } -> std::same_as<void>;
    { b.bulk_load(batch, commit_every) } -> std::same_as<void>;
    { b.get(id) } -> std::same_as<std::optional<record>>;
    { b.range_scan(pred) } -> std::same_as<std::vector<record>>;
    { b.checkpoint() } -> std::same_as<void>;
    { b.close() } -> std::same_as<void>;
    { b.reopen() } -> std::same_as<void>;
    { cb.disk_bytes() } -> std::same_as<std::uint64_t>;
};

struct summary {
    double mean_us{};
    double p50_us{};
    double p95_us{};
    double p99_us{};
    double throughput_per_s{};
};

inline double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(p * static_cast<double>(values.size() - 1));
    return values[index];
}

// Discards the first `warmup` samples before computing percentiles/throughput, per the
// warm-up-discard rule in docs/benchmarks.md.
inline summary summarize(std::vector<double> samples, std::size_t warmup = 0) {
    if (samples.size() > warmup) samples.erase(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(warmup));
    const double total = std::accumulate(samples.begin(), samples.end(), 0.0);
    const double mean = samples.empty() ? 0.0 : total / static_cast<double>(samples.size());
    return {mean, percentile(samples, 0.50), percentile(samples, 0.95), percentile(samples, 0.99),
            mean == 0 ? 0 : 1'000'000.0 / mean};
}

struct csv_row {
    std::string engine;
    adapter_kind kind;
    std::string workload;
    durability_profile profile;
    std::string dataset_size;
    std::size_t iterations{};
    summary stats;
    std::uint64_t disk_bytes{};
    double checkpoint_us{-1};
    double recovery_us{-1};
    std::string notes;
};

struct csv_writer {
    explicit csv_writer(const std::filesystem::path& path) : out_(path) {
        out_ << "engine,adapter_kind,workload,durability_profile,dataset_size,iterations,"
                "mean_us,p50_us,p95_us,p99_us,throughput_per_s,disk_bytes,checkpoint_us,recovery_us,notes\n";
    }
    void write(const csv_row& row) {
        out_ << row.engine << ',' << to_string(row.kind) << ',' << row.workload << ','
             << to_string(row.profile) << ',' << row.dataset_size << ',' << row.iterations << ','
             << row.stats.mean_us << ',' << row.stats.p50_us << ',' << row.stats.p95_us << ','
             << row.stats.p99_us << ',' << row.stats.throughput_per_s << ',' << row.disk_bytes << ','
             << row.checkpoint_us << ',' << row.recovery_us << ',' << '"' << row.notes << '"' << '\n';
        out_.flush();
    }

private:
    std::ofstream out_;
};

inline std::uint64_t directory_bytes(const std::filesystem::path& dir) {
    std::uint64_t total = 0;
    if (!std::filesystem::exists(dir)) return 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) total += static_cast<std::uint64_t>(entry.file_size());
    }
    return total;
}

} // namespace opheap_bench
