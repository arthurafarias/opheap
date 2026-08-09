#include <opheap/opheap.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {
using clock_type = std::chrono::steady_clock;

struct summary {
    double mean_us{};
    double p50_us{};
    double p95_us{};
    double p99_us{};
    double throughput_per_s{};
};

double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(p * static_cast<double>(values.size() - 1));
    return values[index];
}

summary summarize(const std::vector<double>& samples) {
    const double total = std::accumulate(samples.begin(), samples.end(), 0.0);
    const double mean = samples.empty() ? 0.0 : total / samples.size();
    return {
        mean,
        percentile(samples, 0.50),
        percentile(samples, 0.95),
        percentile(samples, 0.99),
        mean == 0 ? 0 : 1'000'000.0 / mean
    };
}

void print(std::string_view name, const summary& s) {
    std::cout << std::left << std::setw(28) << name
              << " mean=" << std::fixed << std::setprecision(2) << s.mean_us << "us"
              << " p50=" << s.p50_us << "us"
              << " p95=" << s.p95_us << "us"
              << " p99=" << s.p99_us << "us"
              << " ops/s=" << std::setprecision(0) << s.throughput_per_s << '\n';
}
}

int main(int argc, char** argv) {
    const auto iterations = argc > 1 ? std::max(1, std::stoi(argv[1])) : 500;
    const auto directory = std::filesystem::temp_directory_path() / "opheap-benchmark";
    std::filesystem::remove_all(directory);

    std::vector<double> commit_samples;
    commit_samples.reserve(static_cast<std::size_t>(iterations));

    auto heap = opheap::heap::open({.path = directory});
    {
        auto seed = heap.begin();
        seed.object_root()["counter"] = 0;
        seed.commit();
    }

    for (int i = 0; i < iterations; ++i) {
        const auto start = clock_type::now();
        auto tx = heap.begin();
        tx.object_root()["counter"] = i;
        tx.commit();
        const auto end = clock_type::now();
        commit_samples.push_back(std::chrono::duration<double, std::micro>(end - start).count());
    }

    const auto checkpoint_start = clock_type::now();
    heap.checkpoint();
    const auto checkpoint_us = std::chrono::duration<double, std::micro>(clock_type::now() - checkpoint_start).count();
    heap.close();

    const auto recovery_start = clock_type::now();
    auto reopened = opheap::heap::open({.path = directory});
    const auto recovery_us = std::chrono::duration<double, std::micro>(clock_type::now() - recovery_start).count();

    const auto durable = summarize(commit_samples);
    print("small durable transaction", durable);
    std::cout << "checkpoint=" << checkpoint_us << "us\n";
    std::cout << "recovery=" << recovery_us << "us\n";

    std::ofstream csv{directory / "summary.csv"};
    csv << "workload,iterations,mean_us,p50_us,p95_us,p99_us,throughput_per_s,checkpoint_us,recovery_us,durability\n";
    csv << "small_durable_transaction," << iterations << ','
        << durable.mean_us << ',' << durable.p50_us << ',' << durable.p95_us << ',' << durable.p99_us << ','
        << durable.throughput_per_s << ',' << checkpoint_us << ',' << recovery_us << ",strict\n";
    std::cout << "csv=" << (directory / "summary.csv") << '\n';
}
