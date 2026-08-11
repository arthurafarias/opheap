#pragma once

#include <ostream>

namespace opheap::module::cli {

inline void print_usage(std::ostream& output) {
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

} // namespace opheap::module::cli
