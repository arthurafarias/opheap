#pragma once

#include "opheap/memory_resource.hpp"
#include "opheap/storage.hpp"
#include "opheap/types.hpp"
#include "opheap/value.hpp"

#include <memory>
#include <memory_resource>
#include <string_view>

namespace opheap {

namespace detail { class heap_state; }

class transaction final : public change_observer {
public:
    transaction() = delete;
    transaction(transaction&&) noexcept;
    transaction& operator=(transaction&&) noexcept;
    transaction(const transaction&) = delete;
    transaction& operator=(const transaction&) = delete;
    ~transaction() override;

    // Universal persistent root. Nested object/array/scalar state is expressed through value.
    value& root(std::string_view name = "root");
    object& object_root(std::string_view name = "root");

    void commit();
    void abort() noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] transaction_id id() const noexcept;
    [[nodiscard]] std::size_t dirty_roots() const noexcept;

    void changed(const change_event& event) noexcept override;

private:
    friend class heap;
    explicit transaction(std::shared_ptr<detail::heap_state> state);
    struct implementation;
    std::unique_ptr<implementation> impl_;
};

class heap {
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
    void close() noexcept;

private:
    explicit heap(std::shared_ptr<detail::heap_state> state) : state_(std::move(state)) {}
    std::shared_ptr<detail::heap_state> state_;
};

} // namespace opheap
