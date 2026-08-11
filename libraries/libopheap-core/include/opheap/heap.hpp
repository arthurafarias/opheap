#pragma once

#include "opheap/cache_info.hpp"
#include "opheap/heap_config.hpp"
#include "opheap/integrity_report.hpp"
#include "opheap/make_default_storage_backend.hpp"
#include "opheap/storage_backend.hpp"
#include "opheap/transaction.hpp"

#include <cstddef>
#include <memory>

namespace opheap {

namespace detail { struct heap_state; }

struct heap {
public:
    static heap open(heap_config config,
                     std::shared_ptr<storage_backend> storage = make_default_storage_backend());

    heap(heap&&) noexcept = default;
    heap& operator=(heap&&) noexcept = default;
    heap(const heap&) = delete;
    heap& operator=(const heap&) = delete;
    ~heap();

    [[nodiscard]] transaction begin();
    void checkpoint();
    [[nodiscard]] integrity_report check_integrity() const noexcept;
    [[nodiscard]] std::size_t root_count() const noexcept;
    [[nodiscard]] cache_info cache() const noexcept;
    void close() noexcept;

private:
    explicit heap(std::shared_ptr<detail::heap_state> state) : state_(std::move(state)) {}
    std::shared_ptr<detail::heap_state> state_;
};

} // namespace opheap
