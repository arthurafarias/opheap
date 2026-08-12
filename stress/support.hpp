#pragma once

// Shared plumbing for the opheap_stress runner: CLI-tunable scale/seed, and a
// scenario-registration/reporting harness mirroring opheap::testing::run_all(),
// but for long-running scenarios timed and run standalone (never through ctest).

#include <opheap/testing/test_context.hpp>
#include <opheap/testing/test_failure.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace opheap::stress {

struct options {
    // Multiplies default iteration/thread/payload counts. 1 = a few seconds per
    // scenario; raise it (--scale N) for a real overnight soak run.
    unsigned scale{1};
    std::uint64_t seed{std::random_device{}()};
    std::vector<std::string> only;
};

inline bool should_run(const options& opts, std::string_view name) {
    if (opts.only.empty()) return true;
    for (const auto& wanted : opts.only) if (wanted == name) return true;
    return false;
}

using scenario_fn = std::function<void(const options&, opheap::testing::test_context&)>;

struct scenario {
    std::string name;
    scenario_fn run;
};

inline std::vector<scenario>& registry() {
    static std::vector<scenario> scenarios;
    return scenarios;
}

struct registrar {
    registrar(std::string name, scenario_fn fn) {
        registry().push_back({std::move(name), std::move(fn)});
    }
};

inline int run_all(const options& opts) {
    opheap::testing::test_context ctx;
    std::size_t passed = 0;
    std::size_t failed = 0;
    for (const auto& s : registry()) {
        if (!should_run(opts, s.name)) continue;
        std::cout << "[RUN ] " << s.name << " (scale=" << opts.scale << ", seed=" << opts.seed << ")\n";
        const auto start = std::chrono::steady_clock::now();
        try {
            s.run(opts, ctx);
            const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::cout << "[PASS] " << s.name << " (" << elapsed << "s)\n";
            ++passed;
        } catch (const std::exception& error) {
            const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::cerr << "[FAIL] " << s.name << " (" << elapsed << "s) - " << error.what() << '\n';
            ++failed;
        } catch (...) {
            std::cerr << "[FAIL] " << s.name << " - unknown exception\n";
            ++failed;
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

} // namespace opheap::stress

#define OPHEAP_STRESS_SCENARIO(name) \
    static void name(const opheap::stress::options& opts, opheap::testing::test_context& ctx); \
    inline static opheap::stress::registrar name##_registrar{#name, name}; \
    static void name(const opheap::stress::options& opts, opheap::testing::test_context& ctx)
