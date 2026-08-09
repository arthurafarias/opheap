#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace opheap {

using root_token = std::uint64_t;
using transaction_id = std::uint64_t;
using sequence_number = std::uint64_t;
using version_type = std::uint64_t;

struct null_t final {
    friend constexpr bool operator==(null_t, null_t) noexcept = default;
};
inline constexpr null_t null{};

class error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class conflict_error : public error { using error::error; };
class corruption_error : public error { using error::error; };
class transaction_error : public error { using error::error; };
class storage_error : public error { using error::error; };
class type_error : public error { using error::error; };

// Strict durability is the default: commit acknowledges only after a persistence barrier.
enum class durability_mode : std::uint8_t {
    strict,
    relaxed
};

struct heap_config {
    std::filesystem::path path;
    std::size_t checkpoint_journal_bytes{64U * 1024U * 1024U};
    durability_mode durability{durability_mode::strict};
    bool checksums{true};
};

struct integrity_report {
    bool ok{true};
    std::size_t roots{};
    std::size_t journal_records{};
    std::uint64_t journal_bytes{};
    sequence_number last_sequence{};
    std::string message{"ok"};
};

} // namespace opheap
