#pragma once

#include "registry.hpp"
#include "test_case.hpp"

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace opheap::testing {

struct test_group {
public:
    test_group(std::string name, std::initializer_list<test_case> tests)
        : name_(std::move(name)), tests_(tests) {
        registry().push_back(this);
    }
    virtual ~test_group() = default;

    [[nodiscard]] std::string_view name() const noexcept { return name_; }
    [[nodiscard]] const std::vector<test_case>& tests() const noexcept { return tests_; }

private:
    std::string name_;
    std::vector<test_case> tests_;
};

} // namespace opheap::testing
