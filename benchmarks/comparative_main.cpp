// Tier A comparative benchmark driver. Runs the same workload matrix (see docs/benchmarks.md)
// against every adapter compiled in (opheap always; SQLite/sqlite_orm/LMDB/RocksDB gated at
// CMake configure time on whether their libraries were found), across both durability profiles,
// and writes one shared-schema CSV plus an environment.json sidecar.

#include "adapters/benchmark_adapter.hpp"
#include "adapters/opheap_adapter.hpp"

#if defined(OPHEAP_BENCH_HAVE_SQLITE)
#include "adapters/sqlite_adapter.hpp"
#endif
#if defined(OPHEAP_BENCH_HAVE_SQLITE_ORM)
#include "adapters/sqlite_orm_adapter.hpp"
#endif
#if defined(OPHEAP_BENCH_HAVE_LMDB)
#include "adapters/lmdb_adapter.hpp"
#endif
#if defined(OPHEAP_BENCH_HAVE_ROCKSDB)
#include "adapters/rocksdb_adapter.hpp"
#include <rocksdb/version.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#if defined(__linux__)
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <unistd.h>
#endif

namespace opheap_bench {
namespace {

using clock_type = std::chrono::steady_clock;

// The harness overrides the library default (8 MiB) down to 2 MiB so the "medium" and "large"
// dataset size classes reliably exceed the cache and exercise eviction/miss paths — see the
// size-class comment in dataset.hpp and the rationale in docs/benchmarks.md.
constexpr std::size_t cache_bytes_override = 2U * 1024U * 1024U;

constexpr std::size_t small_txn_iterations = 200;
constexpr std::size_t small_txn_warmup = 20;
constexpr std::size_t point_read_iterations = 300;
constexpr std::size_t point_read_warmup = 20;
constexpr std::size_t range_scan_warmup = 2;

// Full-scan cost (every adapter here has no secondary index) grows with dataset size, so the
// iteration count shrinks accordingly to keep total runtime bounded.
std::size_t range_scan_iterations_for(const size_class& size) {
    if (size.row_count <= dataset_small.row_count) return 20;
    if (size.row_count <= dataset_medium.row_count) return 10;
    return 4;
}
constexpr std::size_t checkpoint_iterations = 3;
constexpr std::size_t recovery_iterations = 3;
constexpr std::size_t mixed_iterations = 200;
constexpr std::size_t mixed_warmup = 20;

double elapsed_us(clock_type::time_point start) {
    return std::chrono::duration<double, std::micro>(clock_type::now() - start).count();
}

summary aggregate_summary(double total_us, std::size_t ops) {
    const double mean = ops == 0 ? 0.0 : total_us / static_cast<double>(ops);
    return {mean, mean, mean, mean, mean == 0 ? 0.0 : 1'000'000.0 / mean};
}

std::filesystem::path fresh_dir(const std::filesystem::path& root, std::string_view label) {
    auto dir = root / std::string{label};
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

template<backend B>
void run_bulk_read_query_lifecycle(csv_writer& csv, const std::filesystem::path& scratch,
                                    durability_profile profile, const size_class& size) {
    B b;
    b.open(fresh_dir(scratch, size.name), profile, cache_bytes_override);
    const auto rows = generate_dataset(size);

    // bulk_load: single commit for the whole dataset, timed and reported per-row.
    {
        const auto start = clock_type::now();
        b.bulk_load(rows, rows.size());
        const auto total_us = elapsed_us(start);
        csv.write({std::string{B::engine_name()}, B::kind(), "bulk_load", profile, std::string{size.name},
                   rows.size(), aggregate_summary(total_us, rows.size()), b.disk_bytes(), -1, -1, ""});
    }

    // point_read: sample ids spread across the populated dataset.
    {
        std::vector<double> samples;
        const auto sample_count = std::min(point_read_iterations, rows.size());
        const auto stride = std::max<std::size_t>(1, rows.size() / sample_count);
        for (std::size_t i = 0; i < rows.size() && samples.size() < sample_count; i += stride) {
            const auto start = clock_type::now();
            auto found = b.get(rows[i].id);
            samples.push_back(elapsed_us(start));
            if (!found) throw std::runtime_error("point_read: expected row missing");
        }
        csv.write({std::string{B::engine_name()}, B::kind(), "point_read", profile, std::string{size.name},
                   samples.size(), summarize(samples, point_read_warmup), b.disk_bytes(), -1, -1, ""});
    }

    // range_scan: lowest-common-denominator query every engine can express (see tag_predicate).
    {
        std::vector<double> samples;
        const auto iterations = range_scan_iterations_for(size);
        for (std::size_t i = 0; i < iterations; ++i) {
            const auto start = clock_type::now();
            auto matches = b.range_scan({static_cast<std::int32_t>(i % tag_cardinality), 50});
            samples.push_back(elapsed_us(start));
            (void)matches;
        }
        csv.write({std::string{B::engine_name()}, B::kind(), "range_scan", profile, std::string{size.name},
                   samples.size(), summarize(samples, range_scan_warmup), b.disk_bytes(), -1, -1,
                   "equality filter + order-by-seq + limit; no secondary index used"});
    }

    // checkpoint: engine-specific durability-barrier / flush-to-stable-storage operation.
    {
        std::vector<double> samples;
        for (std::size_t i = 0; i < checkpoint_iterations; ++i) {
            const auto start = clock_type::now();
            b.checkpoint();
            samples.push_back(elapsed_us(start));
        }
        const auto stats = summarize(samples, 0);
        csv.write({std::string{B::engine_name()}, B::kind(), "checkpoint", profile, std::string{size.name},
                   samples.size(), stats, b.disk_bytes(), stats.mean_us, -1, ""});
    }

    // recovery: close + reopen, timing only the reopen half.
    {
        std::vector<double> samples;
        for (std::size_t i = 0; i < recovery_iterations; ++i) {
            b.close();
            const auto start = clock_type::now();
            b.reopen();
            samples.push_back(elapsed_us(start));
        }
        const auto stats = summarize(samples, 0);
        csv.write({std::string{B::engine_name()}, B::kind(), "recovery", profile, std::string{size.name},
                   samples.size(), stats, b.disk_bytes(), -1, stats.mean_us, ""});
    }

    b.close();
}

template<backend B>
void run_small_durable_txn(csv_writer& csv, const std::filesystem::path& scratch, durability_profile profile) {
    B b;
    b.open(fresh_dir(scratch, "small_txn"), profile, cache_bytes_override);
    const auto rows = generate_dataset(dataset_small);
    std::vector<double> samples;
    for (std::size_t i = 0; i < small_txn_iterations && i < rows.size(); ++i) {
        const auto start = clock_type::now();
        b.put_commit(rows[i]);
        samples.push_back(elapsed_us(start));
    }
    csv.write({std::string{B::engine_name()}, B::kind(), "small_durable_txn", profile, "n/a", samples.size(),
               summarize(samples, small_txn_warmup), b.disk_bytes(), -1, -1, ""});
    b.close();
}

template<backend B>
void run_bulk_load_per_row_commit(csv_writer& csv, const std::filesystem::path& scratch, durability_profile profile) {
    B b;
    b.open(fresh_dir(scratch, "bulk_per_row"), profile, cache_bytes_override);
    const auto rows = generate_dataset(dataset_small);
    const auto start = clock_type::now();
    b.bulk_load(rows, 1);
    const auto total_us = elapsed_us(start);
    csv.write({std::string{B::engine_name()}, B::kind(), "bulk_load_per_row_commit", profile, "small", rows.size(),
               aggregate_summary(total_us, rows.size()), b.disk_bytes(), -1, -1,
               "one commit per row, contrast with bulk_load's single commit"});
    b.close();
}

template<backend B>
void run_mixed_read_write(csv_writer& csv, const std::filesystem::path& scratch, durability_profile profile) {
    B b;
    b.open(fresh_dir(scratch, "mixed"), profile, cache_bytes_override);
    auto rows = generate_dataset(dataset_small);
    b.bulk_load(rows, rows.size());

    splitmix64 rng{dataset_seed + 1};
    std::vector<double> samples;
    for (std::size_t i = 0; i < mixed_iterations; ++i) {
        const auto id = rows[rng.next() % rows.size()].id;
        const auto start = clock_type::now();
        if (rng.next() % 10 < 8) {
            auto found = b.get(id);
            (void)found;
        } else {
            record updated = rows[static_cast<std::size_t>(id)];
            updated.seq = static_cast<std::int32_t>(rng.next() % 1'000'000);
            b.put_commit(updated);
        }
        samples.push_back(elapsed_us(start));
    }
    csv.write({std::string{B::engine_name()}, B::kind(), "mixed_read_write", profile, "small", samples.size(),
               summarize(samples, mixed_warmup), b.disk_bytes(), -1, -1, "80/20 read/write mix"});
    b.close();
}

template<backend B>
void run_engine(csv_writer& csv, const std::filesystem::path& scratch_root) {
    const auto engine_scratch = scratch_root / std::string{B::engine_name()};
    for (auto profile : {durability_profile::durable, durability_profile::relaxed}) {
        const auto profile_scratch = engine_scratch / std::string{to_string(profile)};
        std::cout << "running " << B::engine_name() << " / " << to_string(profile) << "...\n";
        run_small_durable_txn<B>(csv, profile_scratch, profile);
        for (const auto& size : {dataset_small, dataset_medium, dataset_large}) {
            run_bulk_read_query_lifecycle<B>(csv, profile_scratch, profile, size);
        }
        run_bulk_load_per_row_commit<B>(csv, profile_scratch, profile);
        run_mixed_read_write<B>(csv, profile_scratch, profile);
    }
}

std::string compiler_info() {
#if defined(__clang__)
    return "clang " __clang_version__;
#elif defined(__GNUC__)
    return "gcc " __VERSION__;
#else
    return "unknown";
#endif
}

std::string cpu_model() {
#if defined(__linux__)
    std::ifstream cpuinfo{"/proc/cpuinfo"};
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.starts_with("model name")) {
            const auto colon = line.find(':');
            if (colon != std::string::npos) return line.substr(colon + 2);
        }
    }
#endif
    return "unknown";
}

void write_environment_json(const std::filesystem::path& path) {
    std::ofstream out{path};
    out << "{\n";
    out << "  \"cpu_model\": \"" << cpu_model() << "\",\n";
    out << "  \"hardware_threads\": " << std::thread::hardware_concurrency() << ",\n";
#if defined(__linux__)
    utsname uts{};
    uname(&uts);
    out << "  \"kernel\": \"" << uts.sysname << ' ' << uts.release << ' ' << uts.machine << "\",\n";
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long page_size = sysconf(_SC_PAGE_SIZE);
    out << "  \"ram_bytes\": " << (static_cast<std::int64_t>(pages) * page_size) << ",\n";
#endif
    out << "  \"compiler\": \"" << compiler_info() << "\",\n";
#if defined(OPHEAP_BENCH_HAVE_SQLITE)
    out << "  \"sqlite_version\": \"" << sqlite3_libversion() << "\",\n";
#endif
#if defined(OPHEAP_BENCH_HAVE_LMDB)
    out << "  \"lmdb_version\": \"" << MDB_VERSION_STRING << "\",\n";
#endif
#if defined(OPHEAP_BENCH_HAVE_ROCKSDB)
    out << "  \"rocksdb_version\": \"" << ROCKSDB_MAJOR << "." << ROCKSDB_MINOR << "." << ROCKSDB_PATCH << "\",\n";
#endif
    out << "  \"cache_bytes_override\": " << cache_bytes_override << "\n";
    out << "}\n";
}

// Deliberately not std::filesystem::temp_directory_path(): /tmp is tmpfs (RAM-backed) on many
// Linux systems, where fsync is nearly free — that would silently collapse the "durable" vs.
// "relaxed" durability profiles into the same (meaningless) number. Writing under the current
// working directory keeps output on whatever real filesystem the harness was launched from.
void warn_if_tmpfs(const std::filesystem::path& path) {
#if defined(__linux__)
    struct statfs buf {};
    if (statfs(path.c_str(), &buf) == 0 && buf.f_type == 0x01021994 /* TMPFS_MAGIC */) {
        std::cerr << "warning: " << path
                  << " is on tmpfs — fsync there is nearly free, so the 'durable' profile will not"
                     " reflect real persistent-storage cost. Run from a directory on a real disk.\n";
    }
#endif
}

} // namespace
} // namespace opheap_bench

int main() {
    using namespace opheap_bench;
    const auto output_root = std::filesystem::current_path() / "opheap-comparative-benchmark-output";
    std::filesystem::remove_all(output_root);
    std::filesystem::create_directories(output_root);
    warn_if_tmpfs(output_root);

    csv_writer csv{output_root / "comparative_summary.csv"};

    run_engine<opheap_adapter>(csv, output_root / "scratch");
#if defined(OPHEAP_BENCH_HAVE_SQLITE)
    run_engine<sqlite_adapter>(csv, output_root / "scratch");
#endif
#if defined(OPHEAP_BENCH_HAVE_SQLITE_ORM)
    run_engine<sqlite_orm_adapter>(csv, output_root / "scratch");
#endif
#if defined(OPHEAP_BENCH_HAVE_LMDB)
    run_engine<lmdb_adapter>(csv, output_root / "scratch");
#endif
#if defined(OPHEAP_BENCH_HAVE_ROCKSDB)
    run_engine<rocksdb_adapter>(csv, output_root / "scratch");
#endif

    write_environment_json(output_root / "environment.json");
    std::cout << "csv=" << (output_root / "comparative_summary.csv") << '\n';
    std::cout << "environment=" << (output_root / "environment.json") << '\n';
}
