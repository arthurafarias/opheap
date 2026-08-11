cmake_minimum_required(VERSION 3.24)
project(opheap-sql VERSION 0.2.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/OpheapDependency.cmake)
opheap_require_core()

add_executable(opheap-sql src/main.cpp)
target_link_libraries(opheap-sql PRIVATE opheap::opheap)
target_compile_features(opheap-sql PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(opheap-sql PRIVATE /W4 /permissive-)
else()
    target_compile_options(opheap-sql PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(GNUInstallDirs)
install(TARGETS opheap-sql RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
