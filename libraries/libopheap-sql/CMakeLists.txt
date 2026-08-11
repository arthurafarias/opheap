cmake_minimum_required(VERSION 3.24)
project(opheap-sql-interpreter VERSION 0.1.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/OpheapDependency.cmake)
opheap_require_core()

add_library(opheap-sql-interpreter INTERFACE)
add_library(opheap::sql ALIAS opheap-sql-interpreter)

target_compile_features(opheap-sql-interpreter INTERFACE cxx_std_23)
target_include_directories(opheap-sql-interpreter
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(opheap-sql-interpreter INTERFACE opheap::opheap)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS opheap-sql-interpreter EXPORT opheapSqlTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT opheapSqlTargets NAMESPACE opheap:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-sql)
configure_package_config_file(
    cmake/opheap-sqlConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-sqlConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-sql
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-sqlConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-sqlConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-sqlConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-sql
)
