#pragma once

#include "registry.hpp"
#include "test_context.hpp"
#include "test_group.hpp"

#include <cstddef>
#include <exception>
#include <iostream>

namespace opheap::testing {

inline int run_all() {
    std::size_t passed = 0;
    std::size_t failed = 0;
    test_context context;
    for (const auto* group : registry()) {
        for (const auto& test : group->tests()) {
            try {
                test.run(context);
                ++passed;
                std::cout << "[PASS] " << group->name() << "::" << test.name << '\n';
            } catch (const std::exception& error) {
                ++failed;
                std::cerr << "[FAIL] " << group->name() << "::" << test.name
                          << " - " << error.what() << '\n';
            } catch (...) {
                ++failed;
                std::cerr << "[FAIL] " << group->name() << "::" << test.name
                          << " - unknown exception\n";
            }
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

} // namespace opheap::testing
