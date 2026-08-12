#pragma once

#include "benchmark_adapter.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <string>

namespace opheap_bench {

struct sqlite_adapter {
    static std::string_view engine_name() { return "sqlite"; }
    static adapter_kind kind() { return adapter_kind::embedded_native; }

    void open(const std::filesystem::path& path, durability_profile profile, std::size_t cache_bytes) {
        path_ = path;
        std::filesystem::create_directories(path);
        db_path_ = path / "records.sqlite3";
        open_connection(profile, cache_bytes);
    }

    void put_commit(const record& r) {
        exec("BEGIN IMMEDIATE");
        insert(r);
        exec("COMMIT");
    }

    void bulk_load(const std::vector<record>& batch, std::size_t commit_every) {
        exec("BEGIN IMMEDIATE");
        std::size_t since_commit = 0;
        for (const auto& r : batch) {
            insert(r);
            if (++since_commit >= commit_every) {
                exec("COMMIT");
                exec("BEGIN IMMEDIATE");
                since_commit = 0;
            }
        }
        exec("COMMIT");
    }

    std::optional<record> get(std::int64_t id) {
        sqlite3_stmt* stmt{};
        sqlite3_prepare_v2(db_, "SELECT id, tag, seq, payload FROM records WHERE id = ?", -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, id);
        std::optional<record> result;
        if (sqlite3_step(stmt) == SQLITE_ROW) result = row_to_record(stmt);
        sqlite3_finalize(stmt);
        return result;
    }

    std::vector<record> range_scan(tag_predicate pred) {
        sqlite3_stmt* stmt{};
        sqlite3_prepare_v2(db_, "SELECT id, tag, seq, payload FROM records WHERE tag = ? ORDER BY seq ASC LIMIT ?",
                            -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, pred.tag);
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(pred.limit));
        std::vector<record> matches;
        while (sqlite3_step(stmt) == SQLITE_ROW) matches.push_back(row_to_record(stmt));
        sqlite3_finalize(stmt);
        return matches;
    }

    void checkpoint() { exec("PRAGMA wal_checkpoint(TRUNCATE)"); }

    void close() {
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
    }

    void reopen() {
        close();
        open_connection(profile_, cache_bytes_);
    }

    std::uint64_t disk_bytes() const { return directory_bytes(path_); }

private:
    void open_connection(durability_profile profile, std::size_t cache_bytes) {
        profile_ = profile;
        cache_bytes_ = cache_bytes;
        if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) throw std::runtime_error("sqlite3_open failed");
        exec("PRAGMA journal_mode=WAL");
        exec(profile == durability_profile::durable ? "PRAGMA synchronous=FULL" : "PRAGMA synchronous=OFF");
        // SQLite's cache_size PRAGMA is in pages; assume the default 4096-byte page size.
        exec("PRAGMA cache_size=-" + std::to_string(cache_bytes / 1024));
        exec("CREATE TABLE IF NOT EXISTS records ("
             "id INTEGER PRIMARY KEY, tag INTEGER NOT NULL, seq INTEGER NOT NULL, payload TEXT NOT NULL)");
    }

    void exec(const std::string& sql) {
        char* error{};
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
            std::string message = error ? error : "sqlite3_exec failed";
            sqlite3_free(error);
            throw std::runtime_error(message);
        }
    }

    void insert(const record& r) {
        sqlite3_stmt* stmt{};
        sqlite3_prepare_v2(db_, "INSERT OR REPLACE INTO records (id, tag, seq, payload) VALUES (?, ?, ?, ?)", -1,
                            &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, r.id);
        sqlite3_bind_int(stmt, 2, r.tag);
        sqlite3_bind_int(stmt, 3, r.seq);
        sqlite3_bind_text(stmt, 4, r.payload.data(), static_cast<int>(r.payload.size()), SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    static record row_to_record(sqlite3_stmt* stmt) {
        record r;
        r.id = sqlite3_column_int64(stmt, 0);
        r.tag = sqlite3_column_int(stmt, 1);
        r.seq = sqlite3_column_int(stmt, 2);
        r.payload.assign(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
                          static_cast<std::size_t>(sqlite3_column_bytes(stmt, 3)));
        return r;
    }

    std::filesystem::path path_;
    std::filesystem::path db_path_;
    sqlite3* db_{};
    durability_profile profile_{durability_profile::durable};
    std::size_t cache_bytes_{};
};
static_assert(backend<sqlite_adapter>);

} // namespace opheap_bench
