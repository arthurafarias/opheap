#include <opheap/opheap.hpp>

#include <cstdio>

// Placeholder entry point. opheap-sql will become a server that accepts a SQL
// frontend (parsed statements or a wire protocol, TBD) and executes it against
// an opheap-backed store instead of a conventional relational storage engine.
int main() {
    std::puts("opheap-sql: not yet implemented");
    return 0;
}
