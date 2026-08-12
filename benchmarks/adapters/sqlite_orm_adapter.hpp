#pragma once

// ORM-overhead comparison point: same SQLite engine as sqlite_adapter.hpp, exercised through
// the header-only sqlite_orm C++ ORM instead of raw prepared statements, so the delta between
// the two adapters isolates what the ORM layer costs rather than the underlying engine.

#include "benchmark_adapter.hpp"

#include <sqlite3.h>
#include <sqlite_orm/sqlite_orm.h>

#include <stdexcept>

namespace opheap_bench {

struct orm_record {
    std::int64_t id;
    std::int32_t tag;
    std::int32_t seq;
    std::string payload;
};

inline auto make_orm_storage(const std::string& path) {
    using namespace sqlite_orm;
    return make_storage(path,
        make_table("records",
            make_column("id", &orm_record::id, primary_key()),
            make_column("tag", &orm_record::tag),
            make_column("seq", &orm_record::seq),
            make_column("payload", &orm_record::payload)));
}
using orm_storage_t = decltype(make_orm_storage(std::string{}));

struct sqlite_orm_adapter {
    static std::string_view engine_name() { return "sqlite_orm"; }
    static adapter_kind kind() { return adapter_kind::embedded_orm; }

    void open(const std::filesystem::path& path, durability_profile profile, std::size_t cache_bytes) {
        path_ = path;
        std::filesystem::create_directories(path);
        db_path_ = path / "records_orm.sqlite3";
        profile_ = profile;
        (void)cache_bytes; // sqlite_orm's pragma proxy has no public cache_size wrapper (see docs/benchmarks.md).
        storage_.emplace(make_orm_storage(db_path_.string()));
        storage_->pragma.journal_mode(sqlite_orm::journal_mode::WAL);
        storage_->pragma.synchronous(profile == durability_profile::durable ? 2 : 0);
        storage_->sync_schema(true);
    }

    void put_commit(const record& r) {
        storage_->transaction([&] {
            storage_->replace(to_orm(r));
            return true;
        });
    }

    void bulk_load(const std::vector<record>& batch, std::size_t commit_every) {
        std::size_t offset = 0;
        while (offset < batch.size()) {
            const auto end = std::min(offset + commit_every, batch.size());
            storage_->transaction([&] {
                for (auto i = offset; i < end; ++i) storage_->replace(to_orm(batch[i]));
                return true;
            });
            offset = end;
        }
    }

    std::optional<record> get(std::int64_t id) {
        auto found = storage_->get_pointer<orm_record>(id);
        if (!found) return std::nullopt;
        return from_orm(*found);
    }

    std::vector<record> range_scan(tag_predicate pred) {
        using namespace sqlite_orm;
        auto rows = storage_->get_all<orm_record>(where(c(&orm_record::tag) == pred.tag), order_by(&orm_record::seq),
                                                   limit(static_cast<int>(pred.limit)));
        std::vector<record> matches;
        matches.reserve(rows.size());
        for (const auto& row : rows) matches.push_back(from_orm(row));
        return matches;
    }

    // storage_'s connection handle is not part of sqlite_orm's public API, so checkpointing
    // goes through a throwaway raw connection to the same file — WAL checkpoint is a
    // database-file-level operation, not something owned by a particular ORM session.
    void checkpoint() {
        sqlite3* db{};
        sqlite3_open(db_path_.c_str(), &db);
        sqlite3_exec(db, "PRAGMA wal_checkpoint(TRUNCATE)", nullptr, nullptr, nullptr);
        sqlite3_close(db);
    }

    void close() { storage_.reset(); }

    void reopen() {
        storage_.emplace(make_orm_storage(db_path_.string()));
        storage_->pragma.journal_mode(sqlite_orm::journal_mode::WAL);
        storage_->pragma.synchronous(profile_ == durability_profile::durable ? 2 : 0);
    }

    std::uint64_t disk_bytes() const { return directory_bytes(path_); }

private:
    static orm_record to_orm(const record& r) { return {r.id, r.tag, r.seq, r.payload}; }
    static record from_orm(const orm_record& r) { return {r.id, r.tag, r.seq, r.payload}; }

    std::filesystem::path path_;
    std::filesystem::path db_path_;
    durability_profile profile_{durability_profile::durable};
    std::optional<orm_storage_t> storage_;
};
static_assert(backend<sqlite_orm_adapter>);

} // namespace opheap_bench
