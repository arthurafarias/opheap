#include "opheap/heap.hpp"

#include "opheap/codec.hpp"
#include "opheap/detail/journal.hpp"
#include "opheap/detail/snapshot.hpp"

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace opheap::detail {

class heap_state {
public:
    heap_state(heap_config config, std::shared_ptr<storage_backend> storage)
        : config(std::move(config)), storage(std::move(storage)) {
        if (this->config.path.empty()) throw storage_error("heap path must not be empty");
        this->storage->create_directories(this->config.path);
        snapshot = std::make_unique<snapshot_store>(
            this->config.path / "heap.snapshot", this->storage,
            this->config.durability, this->config.checksums);
        wal = std::make_unique<journal>(
            this->config.path / "heap.wal", this->storage,
            this->config.durability, this->config.checksums);

        auto image = snapshot->load();
        auto recovered = wal->recover(std::move(image.roots), image.sequence, true);
        roots = std::move(recovered.roots);
        last_sequence = recovered.last_sequence;
    }

    struct loaded_root {
        version_type version{};
        std::vector<std::byte> payload;
        bool exists{};
    };

    loaded_root load(std::string_view name) const {
        std::shared_lock lock{roots_mutex};
        auto found = roots.find(std::string{name});
        if (found == roots.end()) return {};
        if (found->second.type != codec<value>::type_name) {
            throw type_error("root type is not opheap::value: " + std::string{name});
        }
        return {found->second.version, found->second.payload, true};
    }

    void commit(transaction_id tx, std::vector<root_update> updates) {
        if (updates.empty()) return;
        std::lock_guard commit_guard{commit_mutex};
        if (closed.load(std::memory_order_acquire)) throw transaction_error("heap is closed");
        if (poisoned.load(std::memory_order_acquire)) throw storage_error("heap is poisoned after a durability failure");

        {
            std::shared_lock lock{roots_mutex};
            for (const auto& update : updates) {
                auto found = roots.find(update.name);
                const auto version = found == roots.end() ? version_type{} : found->second.version;
                if (version != update.expected_version) {
                    throw conflict_error("root version conflict: " + update.name);
                }
            }
        }

        try {
            wal->append(tx, updates);
        } catch (...) {
            poisoned.store(true, std::memory_order_release);
            throw;
        }

        {
            std::unique_lock lock{roots_mutex};
            for (auto& update : updates) {
                roots[update.name] = root_record{
                    update.new_version, std::move(update.type), std::move(update.payload)};
            }
            last_sequence = wal->last_sequence();
        }

        if (wal->size() >= config.checkpoint_journal_bytes) checkpoint_locked();
    }

    void checkpoint() {
        std::lock_guard commit_guard{commit_mutex};
        if (closed.load(std::memory_order_acquire)) throw transaction_error("heap is closed");
        checkpoint_locked();
    }

    void checkpoint_locked() {
        snapshot_image image;
        {
            std::shared_lock lock{roots_mutex};
            image.roots = roots;
            image.sequence = wal->last_sequence();
        }
        snapshot->save(image);
        wal->reset();
        last_sequence = image.sequence;
    }

    integrity_report integrity() const noexcept {
        try {
            auto image = snapshot->load();
            journal verifier{config.path / "heap.wal", storage, config.durability, config.checksums};
            auto recovered = verifier.recover(std::move(image.roots), image.sequence, false);
            integrity_report report;
            report.roots = recovered.roots.size();
            report.journal_records = recovered.records;
            report.journal_bytes = verifier.size();
            report.last_sequence = recovered.last_sequence;
            return report;
        } catch (const std::exception& ex) {
            integrity_report report;
            report.ok = false;
            report.message = ex.what();
            return report;
        }
    }

    heap_config config;
    std::shared_ptr<storage_backend> storage;
    std::unique_ptr<snapshot_store> snapshot;
    std::unique_ptr<journal> wal;
    mutable std::shared_mutex roots_mutex;
    std::mutex commit_mutex;
    std::unordered_map<std::string, root_record> roots;
    std::atomic<transaction_id> next_transaction{1};
    std::atomic<bool> closed{false};
    std::atomic<bool> poisoned{false};
    sequence_number last_sequence{};
};

} // namespace opheap::detail

namespace opheap {

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

transaction::transaction(std::shared_ptr<detail::heap_state> state)
    : impl_(std::make_unique<implementation>(std::move(state))) {}
transaction::transaction(transaction&&) noexcept = default;
transaction& transaction::operator=(transaction&&) noexcept = default;
transaction::~transaction() { abort(); }

value& transaction::root(std::string_view name) {
    if (!impl_ || !impl_->active) throw transaction_error("transaction is not active");
    if (name.empty()) throw transaction_error("root name must not be empty");
    if (auto found = impl_->by_name.find(std::string{name}); found != impl_->by_name.end()) {
        return impl_->roots.at(static_cast<std::size_t>(found->second - 1))->data;
    }

    auto loaded = impl_->state->load(name);
    value root_value{&impl_->pool};
    if (loaded.exists) {
        detail::reader reader{loaded.payload};
        root_value = codec<value>::decode(reader, &impl_->pool);
        if (!reader.eof()) throw corruption_error("root payload contains trailing bytes");
    }

    auto entry = std::make_unique<implementation::working_root>(
        std::string{name}, loaded.version, std::move(root_value));
    const auto token = static_cast<root_token>(impl_->roots.size() + 1);
    entry->data.bind({this, token});
    entry->dirty = !loaded.exists; // creating a previously absent root is a logical mutation
    impl_->by_name.emplace(entry->name, token);
    impl_->roots.push_back(std::move(entry));
    return impl_->roots.back()->data;
}

object& transaction::object_root(std::string_view name) {
    return root(name).as_object();
}

void transaction::changed(const change_event& event) noexcept {
    if (!impl_ || !impl_->active || event.root == 0 || event.root > impl_->roots.size()) return;
    impl_->roots[static_cast<std::size_t>(event.root - 1)]->dirty = true;
}

void transaction::commit() {
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
    impl_->state->commit(impl_->transaction, updates);
    impl_->active = false;
}

void transaction::abort() noexcept {
    if (impl_) impl_->active = false;
}

bool transaction::active() const noexcept { return impl_ && impl_->active; }
transaction_id transaction::id() const noexcept { return impl_ ? impl_->transaction : 0; }
std::size_t transaction::dirty_roots() const noexcept {
    if (!impl_) return 0;
    std::size_t count = 0;
    for (const auto& root : impl_->roots) if (root->dirty) ++count;
    return count;
}

heap heap::open(heap_config config, std::shared_ptr<storage_backend> storage) {
    return heap{std::make_shared<detail::heap_state>(std::move(config), std::move(storage))};
}
heap::~heap() { close(); }
transaction heap::begin() {
    if (!state_ || state_->closed.load(std::memory_order_acquire)) throw transaction_error("heap is closed");
    return transaction{state_};
}
void heap::checkpoint() {
    if (!state_) throw transaction_error("heap is not open");
    state_->checkpoint();
}
integrity_report heap::check_integrity() const noexcept {
    if (!state_) return {false, 0, 0, 0, 0, "heap is not open"};
    return state_->integrity();
}
std::size_t heap::root_count() const noexcept {
    if (!state_) return 0;
    std::shared_lock lock{state_->roots_mutex};
    return state_->roots.size();
}
void heap::close() noexcept {
    if (state_) state_->closed.store(true, std::memory_order_release);
}

} // namespace opheap
