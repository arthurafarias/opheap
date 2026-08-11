#pragma once

#include "opheap/heap.hpp"
#include "opheap/sql/ast.hpp"
#include "opheap/sql/column.hpp"
#include "opheap/sql/parser.hpp"
#include "opheap/sql/result.hpp"
#include "opheap/sql/sql_error.hpp"
#include "opheap/sql/value_bridge.hpp"
#include "opheap/value.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace opheap::sql {

// Maps each SQL table onto its own opheap root, named "sql.table.<name>", holding an
// array of row objects. A single root, "sql.catalog", holds table -> column-list metadata
// so schema can be read back without touching any table's row data.
struct interpreter {
    explicit interpreter(opheap::heap& heap) : heap_(heap) {}

    result execute(std::string_view statement_text) {
        auto parsed = parse_statement(statement_text);
        return std::visit([this](auto&& stmt) { return execute(stmt); }, parsed);
    }

    result list_tables() {
        auto tx = heap_.begin();
        auto& catalog = tx.object_root(catalog_root_name);
        result out;
        out.columns = {"table"};
        for (const auto& [name, definition] : catalog) {
            (void)definition;
            out.rows.push_back({literal{name}});
        }
        return out;
    }

private:
    static constexpr std::string_view catalog_root_name = "sql.catalog";

    static std::string table_root_name(std::string_view table) {
        return "sql.table." + std::string{table};
    }

    static std::vector<column_definition> load_columns(opheap::transaction& tx, std::string_view table) {
        auto& catalog = tx.object_root(catalog_root_name);
        const auto found = catalog.find(std::string{table});
        if (found == catalog.end()) throw sql_error("no such table: " + std::string{table});
        std::vector<column_definition> columns;
        auto& definitions = found->second.as_object().at("columns").as_array();
        for (auto& column : definitions) {
            auto& fields = column.as_object();
            columns.push_back({std::string{fields.at("name").as_string().view()},
                                column_type_from_string(fields.at("type").as_string().view())});
        }
        return columns;
    }

    static bool evaluate(const expression& expr, const opheap::object& row) {
        switch (expr.kind) {
            case expression_kind::comparison: {
                const auto found = row.find(expr.column);
                if (found == row.end()) throw sql_error("no such column: " + expr.column);
                const auto cmp = compare_value_literal(found->second, expr.value);
                if (!cmp) return false;
                switch (expr.comparator) {
                    case comparison_operator::eq: return *cmp == 0;
                    case comparison_operator::neq: return *cmp != 0;
                    case comparison_operator::lt: return *cmp < 0;
                    case comparison_operator::lte: return *cmp <= 0;
                    case comparison_operator::gt: return *cmp > 0;
                    case comparison_operator::gte: return *cmp >= 0;
                }
                throw sql_error("malformed comparison");
            }
            case expression_kind::logical:
                return expr.logic == logical_operator::and_op
                    ? evaluate(*expr.left, row) && evaluate(*expr.right, row)
                    : evaluate(*expr.left, row) || evaluate(*expr.right, row);
            case expression_kind::negation:
                return !evaluate(*expr.operand, row);
        }
        throw sql_error("malformed expression");
    }

    result execute(const create_table_statement& stmt) {
        auto tx = heap_.begin();
        auto& catalog = tx.object_root(catalog_root_name);
        if (catalog.contains(stmt.table)) throw sql_error("table already exists: " + stmt.table);

        auto& entry = catalog[stmt.table];
        auto& definitions = entry["columns"].as_array();
        for (const auto& column : stmt.columns) {
            opheap::value def;
            def["name"] = column.name;
            def["type"] = to_string(column.type);
            definitions.push_back(std::move(def));
        }
        tx.root(table_root_name(stmt.table)).as_array();
        tx.commit();
        return result{.message = "CREATE TABLE"};
    }

    result execute(const insert_statement& stmt) {
        auto tx = heap_.begin();
        const auto columns = load_columns(tx, stmt.table);

        std::vector<std::string> target_columns = stmt.columns;
        if (target_columns.empty()) {
            for (const auto& column : columns) target_columns.push_back(column.name);
        }
        std::unordered_map<std::string, column_type> type_by_name;
        for (const auto& column : columns) type_by_name.emplace(column.name, column.type);
        for (const auto& name : target_columns) {
            if (!type_by_name.contains(name)) throw sql_error("no such column: " + name);
        }

        auto& table = tx.root(table_root_name(stmt.table)).as_array();
        std::size_t inserted = 0;
        for (const auto& tuple : stmt.rows) {
            if (tuple.size() != target_columns.size())
                throw sql_error("value count does not match column count");

            opheap::value row;
            auto& fields = row.as_object();
            for (const auto& column : columns) fields[column.name] = opheap::null;
            for (std::size_t i = 0; i < tuple.size(); ++i) {
                const auto& name = target_columns[i];
                auto coerced = coerce(tuple[i], type_by_name.at(name), name);
                assign_literal(fields[name], coerced);
            }
            table.push_back(std::move(row));
            ++inserted;
        }
        tx.commit();
        return result{.message = "INSERT " + std::to_string(inserted), .rows_affected = inserted};
    }

    result execute(const select_statement& stmt) {
        auto tx = heap_.begin();
        const auto columns = load_columns(tx, stmt.table);

        std::vector<std::string> projection = stmt.columns;
        if (projection.empty()) {
            for (const auto& column : columns) projection.push_back(column.name);
        } else {
            for (const auto& name : projection)
                if (!contains_column(columns, name)) throw sql_error("no such column: " + name);
        }
        if (stmt.order_by_column && !contains_column(columns, *stmt.order_by_column))
            throw sql_error("no such column: " + *stmt.order_by_column);

        auto& table = tx.root(table_root_name(stmt.table)).as_array();
        std::vector<const opheap::object*> matched;
        for (auto& row : table) {
            auto& fields = row.as_object();
            if (!stmt.where || evaluate(*stmt.where, fields)) matched.push_back(&fields);
        }

        if (stmt.order_by_column) {
            const auto& key = *stmt.order_by_column;
            auto ascending = [&](const opheap::object* a, const opheap::object* b) {
                return less_for_sort(a->at(key), b->at(key));
            };
            if (stmt.order_direction == sort_direction::ascending) {
                std::stable_sort(matched.begin(), matched.end(), ascending);
            } else {
                std::stable_sort(matched.begin(), matched.end(),
                                  [&](auto* a, auto* b) { return ascending(b, a); });
            }
        }
        if (stmt.limit && *stmt.limit >= 0 && static_cast<std::size_t>(*stmt.limit) < matched.size()) {
            matched.resize(static_cast<std::size_t>(*stmt.limit));
        }

        result out;
        out.columns = projection;
        for (const auto* fields : matched) {
            std::vector<literal> line;
            line.reserve(projection.size());
            for (const auto& name : projection) line.push_back(to_literal(fields->at(name)));
            out.rows.push_back(std::move(line));
        }
        out.rows_affected = out.rows.size();
        out.message = "SELECT " + std::to_string(out.rows.size());
        return out;
    }

    result execute(const update_statement& stmt) {
        auto tx = heap_.begin();
        const auto columns = load_columns(tx, stmt.table);
        std::unordered_map<std::string, column_type> type_by_name;
        for (const auto& column : columns) type_by_name.emplace(column.name, column.type);
        for (const auto& [name, value] : stmt.assignments) {
            (void)value;
            if (!type_by_name.contains(name)) throw sql_error("no such column: " + name);
        }

        auto& table = tx.root(table_root_name(stmt.table)).as_array();
        std::size_t updated = 0;
        for (auto& row : table) {
            auto& fields = row.as_object();
            if (stmt.where && !evaluate(*stmt.where, fields)) continue;
            for (const auto& [name, value] : stmt.assignments) {
                auto coerced = coerce(value, type_by_name.at(name), name);
                assign_literal(fields[name], coerced);
            }
            ++updated;
        }
        tx.commit();
        return result{.message = "UPDATE " + std::to_string(updated), .rows_affected = updated};
    }

    result execute(const delete_statement& stmt) {
        auto tx = heap_.begin();
        (void)load_columns(tx, stmt.table);

        auto& table = tx.root(table_root_name(stmt.table)).as_array();
        std::size_t deleted = 0;
        for (auto it = table.begin(); it != table.end();) {
            const auto& fields = it->as_object();
            if (!stmt.where || evaluate(*stmt.where, fields)) {
                it = table.erase(it);
                ++deleted;
            } else {
                ++it;
            }
        }
        tx.commit();
        return result{.message = "DELETE " + std::to_string(deleted), .rows_affected = deleted};
    }

    opheap::heap& heap_;
};

} // namespace opheap::sql
