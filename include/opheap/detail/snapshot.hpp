#pragma once

#include "opheap/detail/journal.hpp"
#include "opheap/storage.hpp"

#include <filesystem>
#include <memory>

namespace opheap::detail {

struct snapshot_image {
    std::unordered_map<std::string, root_record> roots;
    sequence_number sequence{};
};

class snapshot_store {
public:
    snapshot_store(std::filesystem::path path,
                   std::shared_ptr<storage_backend> storage,
                   durability_mode durability,
                   bool checksums)
        : path_(std::move(path)), storage_(std::move(storage)),
          durability_(durability), checksums_(checksums) {}

    [[nodiscard]] snapshot_image load() const;
    void save(const snapshot_image& image) const;

private:
    std::filesystem::path path_;
    std::shared_ptr<storage_backend> storage_;
    durability_mode durability_;
    bool checksums_;
};

} // namespace opheap::detail
