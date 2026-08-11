#pragma once

#include "opheap/codec.hpp"
#include "opheap/detail/cache.hpp"
#include "opheap/detail/journal.hpp"
#include "opheap/detail/snapshot.hpp"
#include "opheap/ids.hpp"
#include "opheap/storage.hpp"
#include "opheap/types.hpp"

#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace opheap::detail {

struct heap_state {
public:
    heap_state(heap_config config, std::shared_ptr<storage_backend> storage)
        : config(std::move(config)), storage(std::move(storage)), cache(this->config.cache_bytes) {
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
        std::shared_ptr<const std::vector<std::byte>> payload;
        bool exists{};
    };

    loaded_root load(std::string_view name) {
        root_record record;
        {
            std::shared_lock lock{roots_mutex};
            auto found = roots.find(std::string{name});
            if (found == roots.end()) return {};
            if (found->second.type != codec<value>::type_name) {
                throw type_error("root type is not opheap::value: " + std::string{name});
            }
            record = found->second;
        }

        if (auto cached = cache.get(name, record.version)) {
            return {record.version, std::move(cached), true};
        }

        // A cache miss must pin the locator generation against checkpoint/WAL reclaim.
        // Re-read metadata after taking the commit lock in case a commit or checkpoint
        // changed the root between the optimistic lookup above and this point.
        std::lock_guard commit_guard{commit_mutex};
        {
            std::shared_lock lock{roots_mutex};
            auto found = roots.find(std::string{name});
            if (found == roots.end()) return {};
            record = found->second;
        }

        auto bytes = load_payload(record.payload);
        auto cached = cache.put(std::string{name}, record.version, std::move(bytes));
        return {record.version, std::move(cached), true};
    }

    void read_payload(const root_record& record, std::uint64_t offset,
                      std::span<std::byte> out) const {
        if (record.payload.kind == source::snapshot) snapshot->read(record.payload, offset, out);
        else wal->read(record.payload, offset, out);
    }

    std::vector<std::byte> load_payload(const loc& where) const {
        if (where.kind == source::snapshot) return snapshot->load(where);
        return wal->load(where);
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

        std::vector<loc> locations;
        try {
            locations = wal->append(tx, updates);
        } catch (...) {
            poisoned.store(true, std::memory_order_release);
            throw;
        }

        {
            std::unique_lock lock{roots_mutex};
            for (std::size_t i = 0; i < updates.size(); ++i) {
                auto& update = updates[i];
                roots[update.name] = root_record{
                    update.new_version, update.type, locations.at(i)};
                [[maybe_unused]] auto retained = cache.put(
                    update.name, update.new_version, std::move(update.payload));
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

        auto written = snapshot->save(image, [this](const root_record& record,
                                                    std::uint64_t offset,
                                                    std::span<std::byte> out) {
            read_payload(record, offset, out);
        });

        {
            std::unique_lock lock{roots_mutex};
            roots = std::move(written.roots);
            last_sequence = written.sequence;
        }
        wal->reset();
    }

    integrity_report integrity() const noexcept {
        try {
            auto image = snapshot->load();
            journal verifier{config.path / "heap.wal", storage, config.durability, config.checksums};
            auto recovered = verifier.recover(std::move(image.roots), image.sequence, false);

            for (const auto& [name, record] : recovered.roots) {
                (void)name;
                if (record.payload.kind == source::snapshot) {
                    [[maybe_unused]] auto payload = snapshot->load(record.payload);
                } else {
                    [[maybe_unused]] auto payload = verifier.load(record.payload);
                }
            }

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
    payload_cache cache;
    mutable std::shared_mutex roots_mutex;
    std::mutex commit_mutex;
    std::unordered_map<std::string, root_record> roots;
    std::atomic<transaction_id> next_transaction{1};
    std::atomic<bool> closed{false};
    std::atomic<bool> poisoned{false};
    sequence_number last_sequence{};
};

} // namespace opheap::detail
