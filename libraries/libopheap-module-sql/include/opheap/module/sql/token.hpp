#pragma once

#include "opheap/module/sql/token_kind.hpp"

#include <cstdint>
#include <string>

namespace opheap::module::sql {

struct token {
    token_kind kind{token_kind::end};
    std::string text{};
    std::int64_t integer_value{};
    double real_value{};
};

} // namespace opheap::module::sql
