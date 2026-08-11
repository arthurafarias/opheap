#pragma once

#include "opheap/storage.hpp"
#include "opheap/types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace opheap::detail {

enum class source : std::uint8_t { snapshot, journal };

struct loc {
    source kind{};
    std::uint64_t offset{};
    std::uint64_t size{};
    std::uint32_t checksum{};
};

struct root_record {
    version_type version{};
    std::string type;
    loc payload;
};

struct root_update {
    std::string name;
    version_type expected_version{};
    version_type new_version{};
    std::string type;
    std::vector<std::byte> payload;
};

struct recovery_result {
    std::unordered_map<std::string, root_record> roots;
    sequence_number last_sequence{};
    std::size_t records{};
    std::uint64_t valid_bytes{};
};

class journal {
public:
    journal(std::filesystem::path path,
            std::shared_ptr<storage_backend> storage,
            durability_mode durability,
            bool checksums);

    recovery_result recover(std::unordered_map<std::string, root_record> base,
                            sequence_number snapshot_sequence,
                            bool truncate_torn_tail);

    [[nodiscard]] std::vector<loc> append(transaction_id tx,
                                          const std::vector<root_update>& updates);

    void read(const loc& where, std::uint64_t offset, std::span<std::byte> out) const;
    [[nodiscard]] std::vector<std::byte> load(const loc& where) const;

    void reset();

    [[nodiscard]] sequence_number last_sequence() const noexcept { return last_sequence_; }
    [[nodiscard]] std::uint64_t size() const { return file_->size(); }

private:
    std::filesystem::path path_;
    std::shared_ptr<storage_backend> storage_;
    std::unique_ptr<storage_file> file_;
    durability_mode durability_;
    bool checksums_;
    sequence_number last_sequence_{};
};

} // namespace opheap::detail
