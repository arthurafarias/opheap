add_executable(opheap_benchmark benchmark.cpp)
target_link_libraries(opheap_benchmark PRIVATE opheap::opheap)
target_compile_features(opheap_benchmark PRIVATE cxx_std_23)

# Comparative suite (opheap vs. SQLite/sqlite_orm/LMDB/RocksDB, see docs/benchmarks.md). Each
# engine is independently detected and skipped if not found, following the same
# find_package(... QUIET) + graceful `return()`-free skip precedent as
# examples/beast_credentials/CMakeLists.txt, so the harness degrades gracefully rather than
# being all-or-nothing.
if(OPHEAP_BUILD_BENCHMARKS_COMPARATIVE)
    add_executable(opheap_benchmark_comparative comparative_main.cpp)
    target_link_libraries(opheap_benchmark_comparative PRIVATE opheap::opheap)
    target_compile_features(opheap_benchmark_comparative PRIVATE cxx_std_23)
    target_include_directories(opheap_benchmark_comparative PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        target_compile_definitions(opheap_benchmark_comparative PRIVATE OPHEAP_BENCH_HAVE_SQLITE)
        target_link_libraries(opheap_benchmark_comparative PRIVATE SQLite::SQLite3)
        message(STATUS "benchmarks/comparative: sqlite adapter enabled")

        # sqlite_orm is a single header-only library not packaged by most distros (unlike
        # SQLite/LMDB/RocksDB, which are system libraries below) — vendored via FetchContent.
        include(FetchContent)
        FetchContent_Declare(sqlite_orm
            GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
            GIT_TAG v1.9.1
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(sqlite_orm)
        target_compile_definitions(opheap_benchmark_comparative PRIVATE OPHEAP_BENCH_HAVE_SQLITE_ORM)
        target_link_libraries(opheap_benchmark_comparative PRIVATE sqlite_orm::sqlite_orm)
        message(STATUS "benchmarks/comparative: sqlite_orm adapter enabled")
    else()
        message(STATUS "benchmarks/comparative: sqlite and sqlite_orm adapters skipped (SQLite3 not found)")
    endif()

    # No .pc file ships for liblmdb here, so find_package/pkg-config alone won't locate it.
    find_path(LMDB_INCLUDE_DIR NAMES lmdb.h)
    find_library(LMDB_LIBRARY NAMES lmdb)
    if(LMDB_INCLUDE_DIR AND LMDB_LIBRARY)
        target_compile_definitions(opheap_benchmark_comparative PRIVATE OPHEAP_BENCH_HAVE_LMDB)
        target_include_directories(opheap_benchmark_comparative PRIVATE "${LMDB_INCLUDE_DIR}")
        target_link_libraries(opheap_benchmark_comparative PRIVATE "${LMDB_LIBRARY}")
        message(STATUS "benchmarks/comparative: lmdb adapter enabled")
    else()
        message(STATUS "benchmarks/comparative: lmdb adapter skipped (liblmdb not found)")
    endif()

    find_package(RocksDB CONFIG QUIET)
    if(NOT RocksDB_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(ROCKSDB QUIET rocksdb)
        endif()
    endif()
    if(RocksDB_FOUND)
        target_compile_definitions(opheap_benchmark_comparative PRIVATE OPHEAP_BENCH_HAVE_ROCKSDB)
        target_link_libraries(opheap_benchmark_comparative PRIVATE RocksDB::rocksdb)
        message(STATUS "benchmarks/comparative: rocksdb adapter enabled (CMake package config)")
    elseif(ROCKSDB_FOUND)
        target_compile_definitions(opheap_benchmark_comparative PRIVATE OPHEAP_BENCH_HAVE_ROCKSDB)
        target_include_directories(opheap_benchmark_comparative PRIVATE ${ROCKSDB_INCLUDE_DIRS})
        target_link_libraries(opheap_benchmark_comparative PRIVATE ${ROCKSDB_LIBRARIES})
        message(STATUS "benchmarks/comparative: rocksdb adapter enabled (pkg-config)")
    else()
        message(STATUS "benchmarks/comparative: rocksdb adapter skipped (rocksdb not found)")
    endif()
endif()
