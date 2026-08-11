#pragma once

#include "opheap/module/sql/literal.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace opheap::module::sql {

struct result {
    std::string message{};
    std::vector<std::string> columns{};
    std::vector<std::vector<literal>> rows{};
    std::size_t rows_affected{0};
};

} // namespace opheap::module::sql
