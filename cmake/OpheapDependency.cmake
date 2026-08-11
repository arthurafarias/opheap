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

# Resolves the opheap::module::sql target for a utility application under applications/.
# Mirrors opheap_require_core(): reuse an existing target, else find_package(),
# else fall back to the in-tree libopheap-module-sql so the application still builds
# standalone on a machine with nothing installed.
function(opheap_require_module_sql)
    if(TARGET opheap::module::sql)
        return()
    endif()

    find_package(opheap-module-sql CONFIG QUIET)
    if(TARGET opheap::module::sql)
        return()
    endif()

    add_subdirectory(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../libraries/libopheap-module-sql
        ${CMAKE_BINARY_DIR}/libopheap-module-sql
    )
endfunction()

# Resolves the opheap::json target for a utility application under applications/.
# Mirrors opheap_require_core(): reuse an existing target, else find_package(),
# else fall back to the in-tree libopheap-utils-serialization-json so the
# application still builds standalone on a machine with nothing installed.
function(opheap_require_json)
    if(TARGET opheap::json)
        return()
    endif()

    find_package(opheap-utils-serialization-json CONFIG QUIET)
    if(TARGET opheap::json)
        return()
    endif()

    add_subdirectory(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../libraries/libopheap-utils-serialization-json
        ${CMAKE_BINARY_DIR}/libopheap-utils-serialization-json
    )
endfunction()

# Resolves the opheap::module::cli target for a utility application under applications/.
# Mirrors opheap_require_core(): reuse an existing target, else find_package(),
# else fall back to the in-tree libopheap-module-cli so the application still builds
# standalone on a machine with nothing installed.
function(opheap_require_module_cli)
    if(TARGET opheap::module::cli)
        return()
    endif()

    find_package(opheap-module-cli CONFIG QUIET)
    if(TARGET opheap::module::cli)
        return()
    endif()

    add_subdirectory(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../libraries/libopheap-module-cli
        ${CMAKE_BINARY_DIR}/libopheap-module-cli
    )
endfunction()
