#pragma once

#include "opheap/sql/sql_error.hpp"
#include "opheap/sql/token.hpp"

#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace opheap::sql {

namespace detail {

inline const std::unordered_map<std::string, token_kind>& keyword_table() {
    static const std::unordered_map<std::string, token_kind> table{
        {"SELECT", token_kind::kw_select}, {"FROM", token_kind::kw_from}, {"WHERE", token_kind::kw_where},
        {"INSERT", token_kind::kw_insert}, {"INTO", token_kind::kw_into}, {"VALUES", token_kind::kw_values},
        {"UPDATE", token_kind::kw_update}, {"SET", token_kind::kw_set}, {"DELETE", token_kind::kw_delete},
        {"CREATE", token_kind::kw_create}, {"TABLE", token_kind::kw_table},
        {"ORDER", token_kind::kw_order}, {"BY", token_kind::kw_by},
        {"ASC", token_kind::kw_asc}, {"DESC", token_kind::kw_desc}, {"LIMIT", token_kind::kw_limit},
        {"AND", token_kind::kw_and}, {"OR", token_kind::kw_or}, {"NOT", token_kind::kw_not},
        {"NULL", token_kind::kw_null}, {"TRUE", token_kind::kw_true}, {"FALSE", token_kind::kw_false},
        {"INTEGER", token_kind::kw_integer}, {"INT", token_kind::kw_integer},
        {"REAL", token_kind::kw_real}, {"DOUBLE", token_kind::kw_real},
        {"TEXT", token_kind::kw_text},
        {"BOOLEAN", token_kind::kw_boolean}, {"BOOL", token_kind::kw_boolean},
    };
    return table;
}

inline std::string to_upper(std::string_view text) {
    std::string upper{text};
    for (auto& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return upper;
}

} // namespace detail

inline std::vector<token> tokenize(std::string_view text) {
    std::vector<token> tokens;
    std::size_t i = 0;
    const std::size_t n = text.size();
    while (i < n) {
        const char c = text[i];
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
        if (c == '-' && i + 1 < n && text[i + 1] == '-') {
            while (i < n && text[i] != '\n') ++i;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const std::size_t start = i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) ++i;
            std::string word{text.substr(start, i - start)};
            const auto upper = detail::to_upper(word);
            const auto& keywords = detail::keyword_table();
            if (const auto found = keywords.find(upper); found != keywords.end()) {
                token t; t.kind = found->second; t.text = std::move(word);
                tokens.push_back(std::move(t));
            } else {
                token t; t.kind = token_kind::identifier; t.text = std::move(word);
                tokens.push_back(std::move(t));
            }
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            const std::size_t start = i;
            bool is_real = false;
            while (i < n && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
            if (i < n && text[i] == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(text[i + 1]))) {
                is_real = true;
                ++i;
                while (i < n && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
            }
            std::string number{text.substr(start, i - start)};
            token t;
            try {
                if (is_real) { t.kind = token_kind::real_literal; t.real_value = std::stod(number); }
                else { t.kind = token_kind::integer_literal; t.integer_value = std::stoll(number); }
            } catch (const std::out_of_range&) {
                throw sql_error("numeric literal out of range: " + number);
            }
            t.text = std::move(number);
            tokens.push_back(std::move(t));
            continue;
        }
        if (c == '\'') {
            std::string value;
            ++i;
            while (true) {
                if (i >= n) throw sql_error("unterminated string literal");
                if (text[i] == '\'') {
                    if (i + 1 < n && text[i + 1] == '\'') { value.push_back('\''); i += 2; continue; }
                    ++i;
                    break;
                }
                value.push_back(text[i]);
                ++i;
            }
            token t; t.kind = token_kind::string_literal; t.text = std::move(value);
            tokens.push_back(std::move(t));
            continue;
        }
        if (c == '!' && i + 1 < n && text[i + 1] == '=') { tokens.push_back({.kind = token_kind::op_neq}); i += 2; continue; }
        if (c == '<' && i + 1 < n && text[i + 1] == '>') { tokens.push_back({.kind = token_kind::op_neq}); i += 2; continue; }
        if (c == '<' && i + 1 < n && text[i + 1] == '=') { tokens.push_back({.kind = token_kind::op_lte}); i += 2; continue; }
        if (c == '>' && i + 1 < n && text[i + 1] == '=') { tokens.push_back({.kind = token_kind::op_gte}); i += 2; continue; }
        switch (c) {
            case ',': tokens.push_back({.kind = token_kind::comma}); ++i; continue;
            case '.': tokens.push_back({.kind = token_kind::dot}); ++i; continue;
            case '*': tokens.push_back({.kind = token_kind::star}); ++i; continue;
            case ';': tokens.push_back({.kind = token_kind::semicolon}); ++i; continue;
            case '(': tokens.push_back({.kind = token_kind::lparen}); ++i; continue;
            case ')': tokens.push_back({.kind = token_kind::rparen}); ++i; continue;
            case '=': tokens.push_back({.kind = token_kind::op_eq}); ++i; continue;
            case '<': tokens.push_back({.kind = token_kind::op_lt}); ++i; continue;
            case '>': tokens.push_back({.kind = token_kind::op_gt}); ++i; continue;
            default: throw sql_error(std::string{"unexpected character '"} + c + "'");
        }
    }
    tokens.push_back({.kind = token_kind::end});
    return tokens;
}

} // namespace opheap::sql
