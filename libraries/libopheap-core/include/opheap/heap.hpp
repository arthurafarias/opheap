#pragma once

#include "opheap/cache_info.hpp"
#include "opheap/detail/heap_state.hpp"
#include "opheap/heap_config.hpp"
#include "opheap/integrity_report.hpp"
#include "opheap/make_default_storage_backend.hpp"
#include "opheap/storage_backend.hpp"
#include "opheap/transaction.hpp"
#include "opheap/transaction_error.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>

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

inline heap heap::open(heap_config config, std::shared_ptr<storage_backend> storage) {
    return heap{std::make_shared<detail::heap_state>(std::move(config), std::move(storage))};
}
inline heap::~heap() { close(); }
inline transaction heap::begin() {
    if (!state_ || state_->closed.load(std::memory_order_acquire)) throw transaction_error("heap is closed");
    return transaction{state_};
}
inline void heap::checkpoint() {
    if (!state_) throw transaction_error("heap is not open");
    state_->checkpoint();
}
inline integrity_report heap::check_integrity() const noexcept {
    if (!state_) return {false, 0, 0, 0, 0, "heap is not open"};
    return state_->integrity();
}
inline std::size_t heap::root_count() const noexcept {
    if (!state_) return 0;
    std::shared_lock lock{state_->roots_mutex};
    return state_->roots.size();
}
inline cache_info heap::cache() const noexcept {
    if (!state_) return {};
    return state_->cache.info();
}
inline void heap::close() noexcept {
    if (state_) state_->closed.store(true, std::memory_order_release);
}

} // namespace opheap
