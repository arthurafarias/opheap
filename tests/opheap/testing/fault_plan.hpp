#pragma once

namespace opheap::testing {

struct fault_plan {
    bool partial_append_once{};
    bool fail_flush_data_once{};
    bool fail_replace_once{};
};

} // namespace opheap::testing
