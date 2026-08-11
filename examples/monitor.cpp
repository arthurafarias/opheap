#include <opheap/opheap.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace {

struct io_monitor {
public:
    struct counters {
        std::uint64_t reads{};
        std::uint64_t read_bytes{};
        std::uint64_t writes{};
        std::uint64_t write_bytes{};
        std::uint64_t appends{};
        std::uint64_t append_bytes{};
        std::uint64_t data_flushes{};
        std::uint64_t full_flushes{};
    };

    void begin(std::string label) {
        std::lock_guard lock{mutex_};
        label_ = std::move(label);
        counters_ = {};
        enabled_ = true;
        std::cout << "\n=== " << label_ << " ===\n";
    }

    counters end() {
        std::lock_guard lock{mutex_};
        enabled_ = false;
        return counters_;
    }

    void read(const std::filesystem::path& file, std::uint64_t offset, std::size_t bytes) {
        std::lock_guard lock{mutex_};
        if (!enabled_) return;
        ++counters_.reads;
        counters_.read_bytes += bytes;
        std::cout << "[io] READ   " << file.filename().string()
                  << " offset=" << offset << " bytes=" << bytes << '\n';
    }

    void write(const std::filesystem::path& file, std::uint64_t offset, std::size_t bytes) {
        std::lock_guard lock{mutex_};
        if (!enabled_) return;
        ++counters_.writes;
        counters_.write_bytes += bytes;
        std::cout << "[io] WRITE  " << file.filename().string()
                  << " offset=" << offset << " bytes=" << bytes << '\n';
    }

    void append(const std::filesystem::path& file, std::size_t bytes) {
        std::lock_guard lock{mutex_};
        if (!enabled_) return;
        ++counters_.appends;
        counters_.append_bytes += bytes;
        std::cout << "[io] APPEND " << file.filename().string()
                  << " bytes=" << bytes << '\n';
    }

    void flush_data(const std::filesystem::path& file) {
        std::lock_guard lock{mutex_};
        if (!enabled_) return;
        ++counters_.data_flushes;
        std::cout << "[io] FLUSH  " << file.filename().string() << " data\n";
    }

    void flush_all(const std::filesystem::path& file) {
        std::lock_guard lock{mutex_};
        if (!enabled_) return;
        ++counters_.full_flushes;
        std::cout << "[io] FSYNC  " << file.filename().string() << " all\n";
    }

private:
    std::mutex mutex_;
    std::string label_;
    counters counters_{};
    bool enabled_{};
};

struct monitored_file final : public opheap::storage_file {
public:
    monitored_file(std::filesystem::path path,
                   std::unique_ptr<opheap::storage_file> inner,
                   std::shared_ptr<io_monitor> monitor)
        : path_(std::move(path)), inner_(std::move(inner)), monitor_(std::move(monitor)) {}

    [[nodiscard]] std::uint64_t size() const override {
        return inner_->size();
    }

    void read_exact(std::uint64_t offset, std::span<std::byte> output) const override {
        monitor_->read(path_, offset, output.size());
        inner_->read_exact(offset, output);
    }

    void write_exact(std::uint64_t offset, std::span<const std::byte> data) override {
        monitor_->write(path_, offset, data.size());
        inner_->write_exact(offset, data);
    }

    [[nodiscard]] std::uint64_t append(std::span<const std::byte> data) override {
        monitor_->append(path_, data.size());
        return inner_->append(data);
    }

    void truncate(std::uint64_t new_size) override {
        inner_->truncate(new_size);
    }

    void flush_data() override {
        monitor_->flush_data(path_);
        inner_->flush_data();
    }

    void flush_all() override {
        monitor_->flush_all(path_);
        inner_->flush_all();
    }

private:
    std::filesystem::path path_;
    std::unique_ptr<opheap::storage_file> inner_;
    std::shared_ptr<io_monitor> monitor_;
};

struct monitored_backend final : public opheap::storage_backend {
public:
    monitored_backend(std::shared_ptr<opheap::storage_backend> inner,
                      std::shared_ptr<io_monitor> monitor)
        : inner_(std::move(inner)), monitor_(std::move(monitor)) {}

    std::unique_ptr<opheap::storage_file>
    open_file(const std::filesystem::path& path, bool create_if_missing) override {
        return std::make_unique<monitored_file>(
            path, inner_->open_file(path, create_if_missing), monitor_);
    }

    void create_directories(const std::filesystem::path& path) override {
        inner_->create_directories(path);
    }

    bool exists(const std::filesystem::path& path) const override {
        return inner_->exists(path);
    }

    void atomic_replace(const std::filesystem::path& from,
                        const std::filesystem::path& to) override {
        inner_->atomic_replace(from, to);
    }

    void remove_file(const std::filesystem::path& path) override {
        inner_->remove_file(path);
    }

    void sync_directory(const std::filesystem::path& directory) override {
        inner_->sync_directory(directory);
    }

private:
    std::shared_ptr<opheap::storage_backend> inner_;
    std::shared_ptr<io_monitor> monitor_;
};

void indent(std::ostream& out, int depth) {
    for (int i = 0; i < depth; ++i) out << "  ";
}

void dump(const opheap::value& value, std::ostream& out, int depth = 0) {
    if (value.is_null()) {
        out << "null";
    } else if (value.is_bool()) {
        out << (value.as_bool() ? "true" : "false");
    } else if (value.is_integer()) {
        out << value.as_integer();
    } else if (value.is_number()) {
        out << value.as_number();
    } else if (value.is_string()) {
        out << std::quoted(std::string{value.as_string().view()});
    } else if (value.is_array()) {
        const auto& array = value.as_array();
        out << "[";
        if (!array.empty()) out << '\n';
        for (std::size_t i = 0; i < array.size(); ++i) {
            indent(out, depth + 1);
            dump(array[i], out, depth + 1);
            if (i + 1 != array.size()) out << ',';
            out << '\n';
        }
        if (!array.empty()) indent(out, depth);
        out << "]";
    } else {
        const auto& object = value.as_object();
        out << "{";
        if (!object.empty()) out << '\n';
        std::size_t index = 0;
        for (const auto& [key, child] : object) {
            indent(out, depth + 1);
            out << std::quoted(key) << ": ";
            dump(child, out, depth + 1);
            if (++index != object.size()) out << ',';
            out << '\n';
        }
        if (!object.empty()) indent(out, depth);
        out << "}";
    }
}

void print_cache(const opheap::heap& heap, std::string_view label) {
    const auto c = heap.cache();
    std::cout << "[cache] " << label
              << " entries=" << c.entries
              << " bytes=" << c.bytes
              << " hits=" << c.hits
              << " misses=" << c.misses
              << " evictions=" << c.evictions << '\n';
}

void print_io(const io_monitor::counters& c) {
    std::cout << "[io-summary] reads=" << c.reads
              << " read_bytes=" << c.read_bytes
              << " appends=" << c.appends
              << " append_bytes=" << c.append_bytes
              << " writes=" << c.writes
              << " write_bytes=" << c.write_bytes
              << " flushes=" << (c.data_flushes + c.full_flushes)
              << '\n';
}

} // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() / "opheap-monitor";
    std::filesystem::remove_all(path);

    auto monitor = std::make_shared<io_monitor>();
    auto backend = std::make_shared<monitored_backend>(
        opheap::make_default_storage_backend(), monitor);

    // Keep the cache intentionally small so the example exposes demand loading.
    opheap::heap_config config{
        .path = path,
        .checkpoint_journal_bytes = 64U * 1024U * 1024U,
        .durability = opheap::durability_mode::strict,
        .checksums = true,
        .cache_bytes = 200,
    };

    auto heap = opheap::heap::open(config, backend);

    monitor->begin("commit three independent roots");
    {
        auto tx = heap.begin();

        auto& users = tx.object_root("users");
        users["arthur"]["name"] = "Arthur";
        users["arthur"]["age"] = 42;
        users["arthur"]["roles"].as_array().push_back(opheap::value{"admin"});
        users["ada"]["name"] = "Ada";
        users["ada"]["age"] = 36;

        auto& settings = tx.object_root("settings");
        settings["sample_rate"] = 48000;
        settings["gain"] = 12;
        settings["enabled"] = true;

        auto& telemetry = tx.object_root("telemetry");
        telemetry["temperature"] = 24.5;
        telemetry["pressure"] = 1013.25;

        std::cout << "[commit] tx=" << tx.id()
                  << " dirty_roots=" << tx.dirty_roots() << '\n';
        tx.commit();
    }
    print_io(monitor->end());
    print_cache(heap, "after commit");

    // Move committed payloads into the snapshot, then reopen. The new heap starts
    // with metadata only; root payloads are not retained in its volatile cache.
    heap.checkpoint();
    heap.close();

    auto reopened = opheap::heap::open(config, backend);
    std::cout << "\nroot metadata count=" << reopened.root_count() << '\n';
    print_cache(reopened, "immediately after reopen");

    monitor->begin("fetch users root on first access");
    {
        auto tx = reopened.begin();
        const auto before = reopened.cache();
        const auto& users = tx.root("users");
        const auto after = reopened.cache();

        std::cout << "[fetch] users cache_delta: entries="
                  << (after.entries - before.entries)
                  << " bytes=" << (after.bytes - before.bytes)
                  << " misses=" << (after.misses - before.misses) << '\n';
        std::cout << "[root:users] decoded tree now resident in this transaction:\n";
        dump(users, std::cout);
        std::cout << '\n';
        tx.commit();
    }
    print_io(monitor->end());
    print_cache(reopened, "after users fetch");

    monitor->begin("fetch users again -- cache hit, no payload read");
    {
        auto tx = reopened.begin();
        const auto& users = tx.root("users");
        std::cout << "[root:users] Arthur age="
                  << users.at("arthur").at("age").as_integer() << '\n';
        tx.commit();
    }
    print_io(monitor->end());
    print_cache(reopened, "after users cache hit");

    monitor->begin("fetch settings root only when requested");
    {
        auto tx = reopened.begin();
        const auto& settings = tx.root("settings");
        std::cout << "[root:settings] decoded tree:\n";
        dump(settings, std::cout);
        std::cout << '\n';
        tx.commit();
    }
    print_io(monitor->end());
    print_cache(reopened, "after settings fetch");

    monitor->begin("modify users and commit to WAL");
    {
        auto tx = reopened.begin();
        auto& users = tx.object_root("users");
        users["arthur"]["age"] = 43;
        users["arthur"]["roles"].as_array().push_back(opheap::value{"maintainer"});

        std::cout << "[commit] tx=" << tx.id()
                  << " dirty_roots=" << tx.dirty_roots() << '\n';
        std::cout << "[root:users] transaction working tree before commit:\n";
        dump(tx.root("users"), std::cout);
        std::cout << '\n';

        tx.commit();
    }
    print_io(monitor->end());
    print_cache(reopened, "after WAL commit");

    monitor->begin("read committed users from cache");
    {
        auto tx = reopened.begin();
        const auto& users = tx.root("users");
        std::cout << "[root:users] committed age="
                  << users.at("arthur").at("age").as_integer() << '\n';
        tx.commit();
    }
    print_io(monitor->end());
    print_cache(reopened, "final");

    std::cout << "\nNotice that telemetry was never fetched after reopen. "
                 "Its locator remains in root metadata, but its payload never entered the cache.\n";
}
