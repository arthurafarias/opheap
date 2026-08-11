#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace opheap::sql {

// Scans `buffer` for a ';' outside of a single-quoted string literal. On success returns
// the statement text (excluding the ';') and the number of bytes consumed (including it),
// so a caller can erase that prefix and keep accumulating the remainder. Returns nullopt
// when the buffer holds no complete statement yet.
[[nodiscard]] inline std::optional<std::pair<std::string, std::size_t>> extract_statement(std::string_view buffer) {
    bool in_string = false;
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        const char c = buffer[i];
        if (c == '\'') {
            in_string = !in_string;
            continue;
        }
        if (c == ';' && !in_string) {
            return std::make_pair(std::string{buffer.substr(0, i)}, i + 1);
        }
    }
    return std::nullopt;
}

} // namespace opheap::sql
