cmake_minimum_required(VERSION 3.24)
project(opheap-browser VERSION 0.2.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/OpheapDependency.cmake)
opheap_require_core()

add_executable(opheap-browser src/main.cpp)
target_link_libraries(opheap-browser PRIVATE opheap::opheap)
target_compile_features(opheap-browser PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(opheap-browser PRIVATE /W4 /permissive-)
else()
    target_compile_options(opheap-browser PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(GNUInstallDirs)
install(TARGETS opheap-browser RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
