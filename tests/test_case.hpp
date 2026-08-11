#pragma once

#include "test_context.hpp"

#include <functional>
#include <string>

namespace opheap::testing {

struct test_case {
    std::string name;
    std::function<void(test_context&)> run;
};

} // namespace opheap::testing
