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
