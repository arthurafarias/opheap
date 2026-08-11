#pragma once

#include <chrono>
#include <cstddef>

namespace credentials {

struct rate_limit_policy {
    std::size_t max_attempts{10};
    std::chrono::steady_clock::duration window{std::chrono::seconds{60}};
};

} // namespace credentials
