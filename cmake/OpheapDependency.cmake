# Resolves the opheap::opheap target for a utility application under applications/.
#
# Resolution order:
#   1. reuse opheap::opheap if an enclosing workspace build already defined it;
#   2. otherwise find_package() whatever opheap is installed on the system, so a
#      utility application can be configured and built entirely on its own;
#   3. otherwise fall back to building the in-tree libraries/libopheap-core, so the
#      application still builds standalone on a machine with nothing installed.
function(opheap_require_core)
    if(TARGET opheap::opheap)
        return()
    endif()

    find_package(opheap CONFIG QUIET)
    if(TARGET opheap::opheap)
        return()
    endif()

    add_subdirectory(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../libraries/libopheap-core
        ${CMAKE_BINARY_DIR}/libopheap-core
    )
endfunction()

# Resolves the opheap::sql target for a utility application under applications/.
# Mirrors opheap_require_core(): reuse an existing target, else find_package(),
# else fall back to the in-tree libopheap-sql so the application still builds
# standalone on a machine with nothing installed.
function(opheap_require_sql)
    if(TARGET opheap::sql)
        return()
    endif()

    find_package(opheap-sql CONFIG QUIET)
    if(TARGET opheap::sql)
        return()
    endif()

    add_subdirectory(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../libraries/libopheap-sql
        ${CMAKE_BINARY_DIR}/libopheap-sql
    )
endfunction()

# Resolves the opheap::cli target for a utility application under applications/.
# Mirrors opheap_require_core(): reuse an existing target, else find_package(),
# else fall back to the in-tree libopheap-cli so the application still builds
# standalone on a machine with nothing installed.
function(opheap_require_cli)
    if(TARGET opheap::cli)
        return()
    endif()

    find_package(opheap-cli CONFIG QUIET)
    if(TARGET opheap::cli)
        return()
    endif()

    add_subdirectory(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../libraries/libopheap-cli
        ${CMAKE_BINARY_DIR}/libopheap-cli
    )
endfunction()
