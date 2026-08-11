cmake_minimum_required(VERSION 3.24)
project(opheap-module-sql-interpreter VERSION 0.1.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/OpheapDependency.cmake)
opheap_require_core()

add_library(opheap-module-sql-interpreter INTERFACE)
add_library(opheap::module::sql ALIAS opheap-module-sql-interpreter)

target_compile_features(opheap-module-sql-interpreter INTERFACE cxx_std_23)
target_include_directories(opheap-module-sql-interpreter
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(opheap-module-sql-interpreter INTERFACE opheap::opheap)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS opheap-module-sql-interpreter EXPORT opheapModuleSqlTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT opheapModuleSqlTargets NAMESPACE opheap:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-module-sql)
configure_package_config_file(
    cmake/opheap-module-sqlConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-module-sqlConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-module-sql
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-module-sqlConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-module-sqlConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-module-sqlConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-module-sql
)
