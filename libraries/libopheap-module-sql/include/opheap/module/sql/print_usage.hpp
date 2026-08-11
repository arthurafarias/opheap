#pragma once

#include <ostream>

namespace opheap::module::sql {

inline void print_usage(std::ostream& output) {
    output << "usage: opheap-sql <database-directory>\n";
}

} // namespace opheap::module::sql
