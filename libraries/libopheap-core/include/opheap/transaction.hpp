#pragma once

#include "opheap/change_event.hpp"
#include "opheap/change_observer.hpp"
#include "opheap/codec.hpp"
#include "opheap/corruption_error.hpp"
#include "opheap/detail/binary.hpp"
#include "opheap/detail/heap_state.hpp"
#include "opheap/ids.hpp"
#include "opheap/transaction_error.hpp"
#include "opheap/value.hpp"

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

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

struct transaction::implementation {
    struct working_root {
        std::string name;
        version_type expected_version{};
        bool dirty{};
        value data;

        working_root(std::string root_name, version_type version, value root_value)
            : name(std::move(root_name)), expected_version(version), data(std::move(root_value)) {}
    };

    explicit implementation(std::shared_ptr<detail::heap_state> heap_state)
        : state(std::move(heap_state)),
          transaction(state->next_transaction.fetch_add(1, std::memory_order_relaxed)),
          pool(std::pmr::pool_options{}, std::pmr::get_default_resource()) {}

    std::shared_ptr<detail::heap_state> state;
    transaction_id transaction{};
    bool active{true};
    std::pmr::unsynchronized_pool_resource pool;
    std::vector<std::unique_ptr<working_root>> roots;
    std::unordered_map<std::string, root_token> by_name;
};

inline transaction::transaction(std::shared_ptr<detail::heap_state> state)
    : impl_(std::make_unique<implementation>(std::move(state))) {}
inline transaction::transaction(transaction&&) noexcept = default;
inline transaction& transaction::operator=(transaction&&) noexcept = default;
inline transaction::~transaction() { abort(); }

inline value& transaction::root(std::string_view name) {
    if (!impl_ || !impl_->active) throw transaction_error("transaction is not active");
    if (name.empty()) throw transaction_error("root name must not be empty");
    if (auto found = impl_->by_name.find(std::string{name}); found != impl_->by_name.end()) {
        return impl_->roots.at(static_cast<std::size_t>(found->second - 1))->data;
    }

    auto loaded = impl_->state->load(name);
    value root_value{&impl_->pool};
    if (loaded.exists) {
        detail::reader reader{std::span<const std::byte>{*loaded.payload}};
        root_value = codec<value>::decode(reader, &impl_->pool);
        if (!reader.eof()) throw corruption_error("root payload contains trailing bytes");
    }

    auto entry = std::make_unique<implementation::working_root>(
        std::string{name}, loaded.version, std::move(root_value));
    const auto token = static_cast<root_token>(impl_->roots.size() + 1);
    entry->data.bind({this, token});
    entry->dirty = !loaded.exists;
    impl_->by_name.emplace(entry->name, token);
    impl_->roots.push_back(std::move(entry));
    return impl_->roots.back()->data;
}

inline object& transaction::object_root(std::string_view name) {
    return root(name).as_object();
}

inline void transaction::changed(const change_event& event) noexcept {
    if (!impl_ || !impl_->active || event.root == 0 || event.root > impl_->roots.size()) return;
    impl_->roots[static_cast<std::size_t>(event.root - 1)]->dirty = true;
}

inline void transaction::commit() {
    if (!impl_ || !impl_->active) throw transaction_error("transaction is not active");
    std::vector<detail::root_update> updates;
    updates.reserve(impl_->roots.size());
    for (const auto& root : impl_->roots) {
        if (!root->dirty) continue;
        detail::writer writer;
        codec<value>::encode(writer, root->data);
        updates.push_back(detail::root_update{
            root->name,
            root->expected_version,
            root->expected_version + 1,
            std::string{codec<value>::type_name},
            std::move(writer).take()});
    }
    impl_->state->commit(impl_->transaction, std::move(updates));
    impl_->active = false;
}

inline void transaction::abort() noexcept {
    if (impl_) impl_->active = false;
}

inline bool transaction::active() const noexcept { return impl_ && impl_->active; }
inline transaction_id transaction::id() const noexcept { return impl_ ? impl_->transaction : 0; }
inline std::size_t transaction::dirty_roots() const noexcept {
    if (!impl_) return 0;
    std::size_t count = 0;
    for (const auto& root : impl_->roots) if (root->dirty) ++count;
    return count;
}

} // namespace opheap
