#pragma once

#include "opheap/detail/journal.hpp"
#include "opheap/storage.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>

namespace opheap::detail {

struct snapshot_image {
    std::unordered_map<std::string, root_record> roots;
    sequence_number sequence{};
};

class snapshot_store {
public:
    using payload_reader = std::function<void(const root_record&, std::uint64_t, std::span<std::byte>)>;

    snapshot_store(std::filesystem::path path,
                   std::shared_ptr<storage_backend> storage,
                   durability_mode durability,
                   bool checksums)
        : path_(std::move(path)), storage_(std::move(storage)),
          durability_(durability), checksums_(checksums) {}

    [[nodiscard]] snapshot_image load() const;
    [[nodiscard]] snapshot_image save(const snapshot_image& image,
                                      const payload_reader& read) const;

    void read(const loc& where, std::uint64_t offset, std::span<std::byte> out) const;
    [[nodiscard]] std::vector<std::byte> load(const loc& where) const;

private:
    std::filesystem::path path_;
    std::shared_ptr<storage_backend> storage_;
    durability_mode durability_;
    bool checksums_;
};

} // namespace opheap::detail
