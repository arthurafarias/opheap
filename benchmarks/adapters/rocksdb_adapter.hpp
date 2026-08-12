#pragma once

#include "benchmark_adapter.hpp"

#include <rocksdb/cache.h>
#include <rocksdb/db.h>
#include <rocksdb/table.h>
#include <rocksdb/write_batch.h>

#include <memory>
#include <stdexcept>
#include <string_view>

namespace opheap_bench {

struct rocksdb_adapter {
    static std::string_view engine_name() { return "rocksdb"; }
    static adapter_kind kind() { return adapter_kind::embedded_native; }

    void open(const std::filesystem::path& path, durability_profile profile, std::size_t cache_bytes) {
        path_ = path;
        profile_ = profile;
        cache_bytes_ = cache_bytes;
        open_db();
    }

    void put_commit(const record& r) {
        rocksdb::WriteOptions options;
        options.sync = profile_ == durability_profile::durable;
        check(db_->Put(options, encode_key(r.id), encode_record(r)));
    }

    void bulk_load(const std::vector<record>& batch, std::size_t commit_every) {
        rocksdb::WriteOptions options;
        options.sync = profile_ == durability_profile::durable;
        rocksdb::WriteBatch write_batch;
        std::size_t since_commit = 0;
        for (const auto& r : batch) {
            write_batch.Put(encode_key(r.id), encode_record(r));
            if (++since_commit >= commit_every) {
                check(db_->Write(options, &write_batch));
                write_batch.Clear();
                since_commit = 0;
            }
        }
        if (write_batch.Count() > 0) check(db_->Write(options, &write_batch));
    }

    std::optional<record> get(std::int64_t id) {
        std::string value;
        const auto status = db_->Get(rocksdb::ReadOptions(), encode_key(id), &value);
        if (status.IsNotFound()) return std::nullopt;
        check(status);
        return decode_record(value);
    }

    // RocksDB has no secondary index either: scan the keyspace and filter client-side, same
    // rationale as opheap_adapter/lmdb_adapter.
    std::vector<record> range_scan(tag_predicate pred) {
        std::unique_ptr<rocksdb::Iterator> it{db_->NewIterator(rocksdb::ReadOptions())};
        std::vector<record> matches;
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            const auto slice = it->value();
            auto rec = decode_record(std::string_view{slice.data(), slice.size()});
            if (rec.tag == pred.tag) matches.push_back(std::move(rec));
        }
        std::sort(matches.begin(), matches.end(), [](const record& a, const record& b) { return a.seq < b.seq; });
        if (matches.size() > pred.limit) matches.resize(pred.limit);
        return matches;
    }

    // Closest RocksDB analog to a checkpoint: force the active memtable to an immutable SST
    // (the LSM-tree equivalent of publishing durable state), not a full manual compaction.
    void checkpoint() { check(db_->Flush(rocksdb::FlushOptions())); }

    void close() {
        db_.reset();
    }

    void reopen() { open_db(); }

    std::uint64_t disk_bytes() const { return directory_bytes(path_); }

private:
    void open_db() {
        rocksdb::Options options;
        options.create_if_missing = true;
        rocksdb::BlockBasedTableOptions table_options;
        table_options.block_cache = rocksdb::NewLRUCache(cache_bytes_);
        options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));
        check(rocksdb::DB::Open(options, path_.string(), &db_));
    }

    static void check(const rocksdb::Status& status) {
        if (!status.ok()) throw std::runtime_error("rocksdb: " + status.ToString());
    }

    std::filesystem::path path_;
    durability_profile profile_{durability_profile::durable};
    std::size_t cache_bytes_{};
    std::unique_ptr<rocksdb::DB> db_;
};
static_assert(backend<rocksdb_adapter>);

} // namespace opheap_bench
