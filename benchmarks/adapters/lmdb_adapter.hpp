#pragma once

#include "benchmark_adapter.hpp"

#include <lmdb.h>

#include <stdexcept>

namespace opheap_bench {

struct lmdb_adapter {
    static std::string_view engine_name() { return "lmdb"; }
    static adapter_kind kind() { return adapter_kind::embedded_native; }

    void open(const std::filesystem::path& path, durability_profile profile, std::size_t cache_bytes) {
        (void)cache_bytes; // LMDB is mmap-backed with no application-level cache to size (see docs/benchmarks.md).
        path_ = path;
        profile_ = profile;
        std::filesystem::create_directories(path);
        open_env(profile);
    }

    void put_commit(const record& r) {
        MDB_txn* txn = begin_write();
        put(txn, r);
        check(mdb_txn_commit(txn), "mdb_txn_commit");
    }

    void bulk_load(const std::vector<record>& batch, std::size_t commit_every) {
        MDB_txn* txn = begin_write();
        std::size_t since_commit = 0;
        for (const auto& r : batch) {
            put(txn, r);
            if (++since_commit >= commit_every) {
                check(mdb_txn_commit(txn), "mdb_txn_commit");
                txn = begin_write();
                since_commit = 0;
            }
        }
        check(mdb_txn_commit(txn), "mdb_txn_commit");
    }

    std::optional<record> get(std::int64_t id) {
        MDB_txn* txn{};
        check(mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn), "mdb_txn_begin");
        const auto key_bytes = encode_key(id);
        MDB_val key{key_bytes.size(), const_cast<char*>(key_bytes.data())};
        MDB_val data{};
        std::optional<record> result;
        const int rc = mdb_get(txn, dbi_, &key, &data);
        if (rc == 0) {
            result = decode_record(std::string_view{static_cast<const char*>(data.mv_data), data.mv_size});
        } else if (rc != MDB_NOTFOUND) {
            mdb_txn_abort(txn);
            check(rc, "mdb_get");
        }
        mdb_txn_abort(txn);
        return result;
    }

    // LMDB has no secondary index: a cursor scan with client-side filtering is the honest
    // baseline cost, same rationale as opheap_adapter::range_scan.
    std::vector<record> range_scan(tag_predicate pred) {
        MDB_txn* txn{};
        check(mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn), "mdb_txn_begin");
        MDB_cursor* cursor{};
        check(mdb_cursor_open(txn, dbi_, &cursor), "mdb_cursor_open");
        std::vector<record> matches;
        MDB_val key{};
        MDB_val data{};
        int rc = mdb_cursor_get(cursor, &key, &data, MDB_FIRST);
        while (rc == 0) {
            auto rec = decode_record(std::string_view{static_cast<const char*>(data.mv_data), data.mv_size});
            if (rec.tag == pred.tag) matches.push_back(std::move(rec));
            rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
        }
        mdb_cursor_close(cursor);
        mdb_txn_abort(txn);
        std::sort(matches.begin(), matches.end(), [](const record& a, const record& b) { return a.seq < b.seq; });
        if (matches.size() > pred.limit) matches.resize(pred.limit);
        return matches;
    }

    // LMDB has no distinct checkpoint/compaction step (single copy-on-write mmap file): the
    // closest analog is a forced msync of already-committed pages.
    void checkpoint() { mdb_env_sync(env_, 1); }

    void close() {
        if (env_) mdb_env_close(env_);
        env_ = nullptr;
    }

    void reopen() {
        close();
        open_env(profile_);
    }

    std::uint64_t disk_bytes() const { return directory_bytes(path_); }

private:
    void open_env(durability_profile profile) {
        check(mdb_env_create(&env_), "mdb_env_create");
        check(mdb_env_set_mapsize(env_, std::size_t{1} << 30), "mdb_env_set_mapsize");
        unsigned flags = 0;
        if (profile == durability_profile::relaxed) flags |= MDB_NOSYNC | MDB_NOMETASYNC;
        check(mdb_env_open(env_, path_.c_str(), flags, 0664), "mdb_env_open");
        MDB_txn* txn{};
        check(mdb_txn_begin(env_, nullptr, 0, &txn), "mdb_txn_begin");
        check(mdb_dbi_open(txn, nullptr, MDB_CREATE, &dbi_), "mdb_dbi_open");
        check(mdb_txn_commit(txn), "mdb_txn_commit");
    }

    MDB_txn* begin_write() {
        MDB_txn* txn{};
        check(mdb_txn_begin(env_, nullptr, 0, &txn), "mdb_txn_begin");
        return txn;
    }

    void put(MDB_txn* txn, const record& r) {
        const auto key_bytes = encode_key(r.id);
        auto value_bytes = encode_record(r);
        MDB_val key{key_bytes.size(), const_cast<char*>(key_bytes.data())};
        MDB_val data{value_bytes.size(), value_bytes.data()};
        check(mdb_put(txn, dbi_, &key, &data, 0), "mdb_put");
    }

    static void check(int rc, std::string_view what) {
        if (rc != 0) throw std::runtime_error(std::string{what} + ": " + mdb_strerror(rc));
    }

    std::filesystem::path path_;
    durability_profile profile_{durability_profile::durable};
    MDB_env* env_{};
    MDB_dbi dbi_{};
};
static_assert(backend<lmdb_adapter>);

} // namespace opheap_bench
