#pragma once

#include "benchmark_adapter.hpp"

#include <opheap/opheap.hpp>

#include <algorithm>
#include <optional>
#include <vector>

namespace opheap_bench {

struct opheap_adapter {
    static std::string_view engine_name() { return "opheap"; }
    static adapter_kind kind() { return adapter_kind::embedded_native; }

    void open(const std::filesystem::path& path, durability_profile profile, std::size_t cache_bytes) {
        path_ = path;
        heap_ = opheap::heap::open({
            .path = path,
            .durability = profile == durability_profile::durable ? opheap::durability_mode::strict
                                                                   : opheap::durability_mode::relaxed,
            .cache_bytes = cache_bytes,
        });
    }

    void put_commit(const record& r) {
        auto tx = heap_->begin();
        write_record(tx.object_root(encode_key(r.id)), r);
        tx.commit();
    }

    void bulk_load(const std::vector<record>& batch, std::size_t commit_every) {
        auto tx = heap_->begin();
        std::size_t since_commit = 0;
        for (const auto& r : batch) {
            write_record(tx.object_root(encode_key(r.id)), r);
            if (++since_commit >= commit_every) {
                tx.commit();
                tx = heap_->begin();
                since_commit = 0;
            }
        }
        tx.commit();
    }

    std::optional<record> get(std::int64_t id) {
        auto tx = heap_->begin();
        try {
            return read_record(tx.object_root(encode_key(id)));
        } catch (const std::out_of_range&) {
            return std::nullopt;
        }
    }

    // opheap's core engine has no secondary index: this honestly reflects that by scanning
    // every resident root and filtering client-side, same as it would cost an application
    // using the raw value-tree API today (opheap-sql's query engine is a separate module not
    // exercised by this adapter).
    std::vector<record> range_scan(tag_predicate pred) {
        std::vector<record> matches;
        auto tx = heap_->begin();
        for (const auto& name : heap_->root_names()) {
            auto r = read_record(tx.object_root(name));
            if (r.tag == pred.tag) matches.push_back(r);
        }
        std::sort(matches.begin(), matches.end(), [](const record& a, const record& b) { return a.seq < b.seq; });
        if (matches.size() > pred.limit) matches.resize(pred.limit);
        return matches;
    }

    void checkpoint() { heap_->checkpoint(); }
    void close() { heap_->close(); }
    void reopen() {
        heap_.reset();
        heap_ = opheap::heap::open({.path = path_});
    }
    std::uint64_t disk_bytes() const { return directory_bytes(path_); }

private:
    static void write_record(opheap::object& root, const record& r) {
        root["id"] = r.id;
        root["tag"] = static_cast<std::int64_t>(r.tag);
        root["seq"] = static_cast<std::int64_t>(r.seq);
        root["payload"] = std::string_view{r.payload};
    }
    static record read_record(const opheap::object& root) {
        record r;
        r.id = root.at("id").as_integer();
        r.tag = static_cast<std::int32_t>(root.at("tag").as_integer());
        r.seq = static_cast<std::int32_t>(root.at("seq").as_integer());
        r.payload = std::string{root.at("payload").as_string()};
        return r;
    }

    std::filesystem::path path_;
    std::optional<opheap::heap> heap_;
};
static_assert(backend<opheap_adapter>);

} // namespace opheap_bench
