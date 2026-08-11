#pragma once

#include "opheap/detail/loc.hpp"
#include "opheap/detail/recovery_result.hpp"
#include "opheap/detail/root_record.hpp"
#include "opheap/detail/root_update.hpp"
#include "opheap/detail/source.hpp"
#include "opheap/ids.hpp"
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

struct journal {
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
