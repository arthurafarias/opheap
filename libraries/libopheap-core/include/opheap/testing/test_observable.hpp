#pragma once

#include <opheap/opheap.hpp>

namespace opheap::testing {

struct test_observable final : public observable {
public:
    void touch() { notify(change_kind::value, 4, 8); }
};

} // namespace opheap::testing
