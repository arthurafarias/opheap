# Boost.Beast credentials example. See README.md and
# docs/credentials-service.md for the intended layering:
#
#   Boost.Beast HTTP -> credential_service -> opheap
#
# opheap itself never depends on Boost or libsodium; only this example does.
find_package(Boost 1.83 QUIET COMPONENTS json)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SODIUM QUIET IMPORTED_TARGET libsodium)
endif()
find_package(Threads QUIET)

if(NOT Boost_FOUND OR NOT TARGET Boost::json)
    message(STATUS "examples/beast_credentials: skipped (Boost::json not found)")
    return()
endif()
if(NOT SODIUM_FOUND)
    message(STATUS "examples/beast_credentials: skipped (libsodium not found)")
    return()
endif()
if(NOT Threads_FOUND)
    message(STATUS "examples/beast_credentials: skipped (Threads not found)")
    return()
endif()

add_library(beast_credentials_service STATIC
    src/credential_service.cpp
    src/password_kdf.cpp
)
target_include_directories(beast_credentials_service
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(beast_credentials_service
    PUBLIC opheap::opheap
    PRIVATE PkgConfig::SODIUM
)
target_compile_features(beast_credentials_service PUBLIC cxx_std_23)
if(MSVC)
    target_compile_options(beast_credentials_service PRIVATE /W4 /permissive-)
else()
    target_compile_options(beast_credentials_service PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(beast_credentials_server
    src/main.cpp
    src/http_router.cpp
)
target_link_libraries(beast_credentials_server PRIVATE
    beast_credentials_service
    Boost::json
    Threads::Threads
)
target_compile_features(beast_credentials_server PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(beast_credentials_server PRIVATE /W4 /permissive-)
else()
    target_compile_options(beast_credentials_server PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(OPHEAP_BUILD_TESTS)
    add_executable(beast_credentials_service_test tests/credential_service_test.cpp)
    target_link_libraries(beast_credentials_service_test PRIVATE beast_credentials_service)
    target_compile_features(beast_credentials_service_test PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(beast_credentials_service_test PRIVATE /W4 /permissive-)
    else()
        target_compile_options(beast_credentials_service_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME beast_credentials_service_test COMMAND beast_credentials_service_test)
endif()
