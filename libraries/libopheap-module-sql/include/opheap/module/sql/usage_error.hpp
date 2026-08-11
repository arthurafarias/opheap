#pragma once

#include <stdexcept>

namespace opheap::module::sql {

// Raised for malformed command-line usage (missing database directory, etc.), as
// opposed to domain errors during a REPL session (sql_error), so run() can tell the
// two apart: usage errors abort startup, domain errors are caught per-statement and
// printed inline so the session continues.
class usage_error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace opheap::module::sql
