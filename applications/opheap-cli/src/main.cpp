#include <opheap/opheap.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

class json_error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

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

void write_string(std::ostream& output, std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    output.put('"');
    for (const unsigned char c : value) {
        switch (c) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (c < 0x20U) {
                output << "\\u00" << hex[c >> 4U] << hex[c & 0x0fU];
            } else {
                output.put(static_cast<char>(c));
            }
        }
    }
    output.put('"');
}

void write_json(std::ostream& output, const opheap::value& value) {
    if (value.is_null()) {
        output << "null";
    } else if (value.is_bool()) {
        output << (value.as_bool() ? "true" : "false");
    } else if (value.is_integer()) {
        output << value.as_integer();
    } else if (value.is_number()) {
        char buffer[64];
        const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value.as_number());
        if (error != std::errc{}) throw std::runtime_error("cannot format number");
        output.write(buffer, end - buffer);
    } else if (value.is_string()) {
        write_string(output, value.as_string().view());
    } else if (value.is_array()) {
        output.put('[');
        bool first = true;
        for (const auto& item : value.as_array()) {
            if (!first) output.put(',');
            first = false;
            write_json(output, item);
        }
        output.put(']');
    } else {
        output.put('{');
        bool first = true;
        for (const auto& [key, item] : value.as_object()) {
            if (!first) output.put(',');
            first = false;
            write_string(output, key);
            output.put(':');
            write_json(output, item);
        }
        output.put('}');
    }
}

std::string read_json_argument(std::string_view argument) {
    if (argument != "-") return std::string{argument};
    return {std::istreambuf_iterator<char>{std::cin}, std::istreambuf_iterator<char>{}};
}

const opheap::value& get_path(opheap::transaction& transaction, std::string_view path) {
    const auto separator = path.find('.');
    const auto root_name = path.substr(0, separator);
    if (root_name.empty()) throw json_error("path must start with a root name");

    const opheap::value* current = &transaction.root(root_name);
    if (current->is_null()) throw std::runtime_error("root not found: " + std::string{root_name});

    std::size_t position = separator;
    while (position != std::string_view::npos) {
        const auto key_start = position + 1;
        position = path.find('.', key_start);
        const auto key = path.substr(key_start, position - key_start);
        if (key.empty()) throw json_error("path contains an empty property name");
        if (!current->is_object())
            throw std::runtime_error("path component is not an object: " + std::string{key});

        const auto& object = current->as_object();
        const auto entry = object.find(std::string{key});
        if (entry == object.end())
            throw std::runtime_error("path property not found: " + std::string{key});
        current = &entry->second;
    }
    return *current;
}

void print_usage(std::ostream& output) {
    output <<
        "usage: opheap-cli [-C <heap-directory>] <command> [arguments]\n"
        "\n"
        "commands:\n"
        "  create <name> <json|->  create a named JSON root\n"
        "  get <path>             print a root or dotted object path as JSON\n"
        "  inspect                print all named roots as a JSON object\n"
        "  update <name> <json|-> replace an existing root\n"
        "  delete <name>          logically delete an existing root\n"
        "  checkpoint             compact the WAL into a snapshot\n"
        "  verify                 verify the snapshot and WAL\n";
}

int run(int argc, char** argv) {
    std::filesystem::path heap_path{"."};
    int argument = 1;
    while (argument < argc) {
        const std::string_view option{argv[argument]};
        if (option != "-C" && option != "--path") break;
        if (++argument == argc) throw json_error(std::string{option} + " requires a directory");
        heap_path = argv[argument++];
    }
    if (argument == argc || std::string_view{argv[argument]} == "help" ||
        std::string_view{argv[argument]} == "--help" || std::string_view{argv[argument]} == "-h") {
        print_usage(std::cout);
        return argument == argc ? 2 : 0;
    }

    const std::string_view command{argv[argument++]};
    auto heap = opheap::heap::open(opheap::heap_config{.path = std::move(heap_path)});

    if (command == "checkpoint") {
        if (argument != argc) throw json_error("checkpoint takes no arguments");
        heap.checkpoint();
        return 0;
    }
    if (command == "verify") {
        if (argument != argc) throw json_error("verify takes no arguments");
        const auto report = heap.check_integrity();
        std::cout << "{\"ok\":" << (report.ok ? "true" : "false")
                  << ",\"roots\":" << report.roots
                  << ",\"journal_records\":" << report.journal_records
                  << ",\"journal_bytes\":" << report.journal_bytes
                  << ",\"last_sequence\":" << report.last_sequence
                  << ",\"message\":";
        write_string(std::cout, report.message);
        std::cout << "}\n";
        return report.ok ? 0 : 1;
    }
    if (command == "inspect") {
        if (argument != argc) throw json_error("inspect takes no arguments");
        auto transaction = heap.begin();
        std::cout.put('{');
        bool first = true;
        for (const auto& name : heap.root_names()) {
            if (!first) std::cout.put(',');
            first = false;
            write_string(std::cout, name);
            std::cout.put(':');
            write_json(std::cout, transaction.root(name));
        }
        std::cout << "}\n";
        return 0;
    }

    if (argument == argc) throw json_error(std::string{command} + " requires a root name");
    const std::string_view name{argv[argument++]};
    if (command == "get") {
        if (argument != argc) throw json_error("get takes one root name");
        auto transaction = heap.begin();
        write_json(std::cout, get_path(transaction, name));
        std::cout.put('\n');
        return 0;
    }
    if (name.empty()) throw json_error("root name must not be empty");
    auto transaction = heap.begin();
    auto& root = transaction.root(name);
    if (command == "delete") {
        if (argument != argc) throw json_error("delete takes one root name");
        if (root.is_null()) throw std::runtime_error("root not found: " + std::string{name});
        root = opheap::null;
        transaction.commit();
        return 0;
    }
    if (command != "create" && command != "update")
        throw json_error("unknown command: " + std::string{command});
    if (argument == argc || argument + 1 != argc)
        throw json_error(std::string{command} + " requires a root name and one JSON value");
    if (command == "create" && !root.is_null())
        throw std::runtime_error("root already exists: " + std::string{name});
    if (command == "update" && root.is_null())
        throw std::runtime_error("root not found: " + std::string{name});

    const auto json = read_json_argument(argv[argument]);
    root = json_parser{json, root.resource()}.parse();
    transaction.commit();
    write_json(std::cout, root);
    std::cout.put('\n');
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const json_error& error) {
        std::cerr << "opheap-cli: " << error.what() << '\n';
        print_usage(std::cerr);
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "opheap-cli: " << error.what() << '\n';
        return 1;
    }
}
