#pragma once

#include "opheap/cli/json_error.hpp"

#include <opheap/value.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>

namespace opheap::cli {

class json_parser final {
public:
    json_parser(std::string_view input, std::pmr::memory_resource* resource)
        : input_(input), resource_(resource) {}

    opheap::value parse() {
        skip_space();
        auto result = parse_value();
        skip_space();
        if (position_ != input_.size()) fail("unexpected trailing input");
        return result;
    }

private:
    [[noreturn]] void fail(std::string_view message) const {
        throw json_error(std::string{message} + " at byte " + std::to_string(position_));
    }

    void skip_space() noexcept {
        while (position_ < input_.size()) {
            const char c = input_[position_];
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') break;
            ++position_;
        }
    }

    bool consume(char expected) noexcept {
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void expect(std::string_view token) {
        if (input_.substr(position_, token.size()) != token) fail("invalid value");
        position_ += token.size();
    }

    opheap::value parse_value() {
        if (position_ == input_.size()) fail("expected a JSON value");
        switch (input_[position_]) {
        case 'n': expect("null"); return opheap::value{opheap::null, resource_};
        case 't': expect("true"); return opheap::value{true, resource_};
        case 'f': expect("false"); return opheap::value{false, resource_};
        case '"': return opheap::value{parse_string(), resource_};
        case '[': return parse_array();
        case '{': return parse_object();
        default: return parse_number();
        }
    }

    static unsigned hex_digit(char c) noexcept {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<unsigned>(c - 'A' + 10);
        return 16;
    }

    std::uint32_t parse_hex_escape() {
        if (input_.size() - position_ < 4) fail("incomplete Unicode escape");
        std::uint32_t code = 0;
        for (int i = 0; i < 4; ++i) {
            const unsigned digit = hex_digit(input_[position_++]);
            if (digit == 16) fail("invalid Unicode escape");
            code = (code << 4U) | digit;
        }
        return code;
    }

    static void append_utf8(std::string& output, std::uint32_t code) {
        if (code <= 0x7fU) {
            output.push_back(static_cast<char>(code));
        } else if (code <= 0x7ffU) {
            output.push_back(static_cast<char>(0xc0U | (code >> 6U)));
            output.push_back(static_cast<char>(0x80U | (code & 0x3fU)));
        } else if (code <= 0xffffU) {
            output.push_back(static_cast<char>(0xe0U | (code >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | (code & 0x3fU)));
        } else {
            output.push_back(static_cast<char>(0xf0U | (code >> 18U)));
            output.push_back(static_cast<char>(0x80U | ((code >> 12U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | (code & 0x3fU)));
        }
    }

    std::string parse_string() {
        if (!consume('"')) fail("expected string");
        std::string output;
        while (position_ < input_.size()) {
            const unsigned char c = static_cast<unsigned char>(input_[position_++]);
            if (c == '"') return output;
            if (c < 0x20U) fail("unescaped control character");
            if (c != '\\') {
                output.push_back(static_cast<char>(c));
                continue;
            }
            if (position_ == input_.size()) fail("incomplete escape");
            switch (input_[position_++]) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                std::uint32_t code = parse_hex_escape();
                if (code >= 0xd800U && code <= 0xdbffU) {
                    if (input_.size() - position_ < 6 || input_[position_] != '\\' ||
                        input_[position_ + 1] != 'u') {
                        fail("missing low surrogate");
                    }
                    position_ += 2;
                    const std::uint32_t low = parse_hex_escape();
                    if (low < 0xdc00U || low > 0xdfffU) fail("invalid low surrogate");
                    code = 0x10000U + ((code - 0xd800U) << 10U) + (low - 0xdc00U);
                } else if (code >= 0xdc00U && code <= 0xdfffU) {
                    fail("unexpected low surrogate");
                }
                append_utf8(output, code);
                break;
            }
            default: fail("invalid escape");
            }
        }
        fail("unterminated string");
    }

    opheap::value parse_array() {
        ++position_;
        opheap::value result{resource_};
        auto& array = result.as_array();
        skip_space();
        if (consume(']')) return result;
        for (;;) {
            skip_space();
            array.push_back(parse_value());
            skip_space();
            if (consume(']')) return result;
            if (!consume(',')) fail("expected ',' or ']'");
        }
    }

    opheap::value parse_object() {
        ++position_;
        opheap::value result{resource_};
        auto& object = result.as_object();
        skip_space();
        if (consume('}')) return result;
        for (;;) {
            skip_space();
            if (position_ == input_.size() || input_[position_] != '"') fail("expected object key");
            auto key = parse_string();
            skip_space();
            if (!consume(':')) fail("expected ':'");
            skip_space();
            auto [entry, inserted] = object.try_emplace(key, resource_);
            if (!inserted) fail("duplicate object key");
            entry->second = parse_value();
            skip_space();
            if (consume('}')) return result;
            if (!consume(',')) fail("expected ',' or '}'");
        }
    }

    opheap::value parse_number() {
        const std::size_t start = position_;
        if (consume('-') && position_ == input_.size()) fail("invalid number");
        if (consume('0')) {
            if (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                fail("leading zero in number");
        } else {
            const std::size_t digits = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                ++position_;
            if (digits == position_) fail("invalid number");
        }
        bool floating = false;
        if (consume('.')) {
            floating = true;
            const std::size_t digits = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                ++position_;
            if (digits == position_) fail("invalid fraction");
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            floating = true;
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-'))
                ++position_;
            const std::size_t digits = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                ++position_;
            if (digits == position_) fail("invalid exponent");
        }
        const auto number = input_.substr(start, position_ - start);
        if (!floating) {
            std::int64_t value{};
            const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), value);
            if (error == std::errc{} && end == number.data() + number.size())
                return opheap::value{value, resource_};
        }
        double value{};
        const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), value);
        if (error != std::errc{} || end != number.data() + number.size() || !std::isfinite(value))
            fail("number is out of range");
        return opheap::value{value, resource_};
    }

    std::string_view input_;
    std::pmr::memory_resource* resource_;
    std::size_t position_{};
};

} // namespace opheap::cli
