#pragma once

#include "opheap/change_event.hpp"
#include "opheap/change_observer.hpp"
#include "opheap/ids.hpp"
#include "opheap/value.hpp"

#include <memory>
#include <string_view>

namespace opheap {

namespace detail { struct heap_state; }

struct transaction final : public change_observer {
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
    friend struct heap;
    explicit transaction(std::shared_ptr<detail::heap_state> state);
    struct implementation;
    std::unique_ptr<implementation> impl_;
};

} // namespace opheap
