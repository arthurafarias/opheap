cmake_minimum_required(VERSION 3.24)
project(opheap-cli VERSION 0.2.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/OpheapDependency.cmake)
opheap_require_core()
opheap_require_module_cli()

add_executable(opheap-cli src/main.cpp)
target_link_libraries(opheap-cli PRIVATE opheap::opheap opheap::module::cli)
target_compile_features(opheap-cli PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(opheap-cli PRIVATE /W4 /permissive-)
else()
    target_compile_options(opheap-cli PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(GNUInstallDirs)
install(TARGETS opheap-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
