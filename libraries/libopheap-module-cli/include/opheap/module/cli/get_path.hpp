#pragma once

#include "opheap/module/cli/usage_error.hpp"

#include <opheap/transaction.hpp>
#include <opheap/value.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace opheap::module::cli {

// Resolves a dotted path like "root.users.arthur" against a transaction's named roots.
inline const opheap::value& get_path(opheap::transaction& transaction, std::string_view path) {
    const auto separator = path.find('.');
    const auto root_name = path.substr(0, separator);
    if (root_name.empty()) throw usage_error("path must start with a root name");

    const opheap::value* current = &transaction.root(root_name);
    if (current->is_null()) throw std::runtime_error("root not found: " + std::string{root_name});

    std::size_t position = separator;
    while (position != std::string_view::npos) {
        const auto key_start = position + 1;
        position = path.find('.', key_start);
        const auto key = path.substr(key_start, position - key_start);
        if (key.empty()) throw usage_error("path contains an empty property name");
        if (!current->is_object())
            throw std::runtime_error("path component is not an object: " + std::string{key});

        const auto& object = current->as_object();
        const auto entry = object.find(std::string{key});
        if (entry == object.end())
            throw std::runtime_error("path property not found: " + std::string{key});
        current = &entry->second;
    }
    return *current;
}

} // namespace opheap::module::cli
