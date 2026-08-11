cmake_minimum_required(VERSION 3.24)
project(opheap-cli-tool VERSION 0.1.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/OpheapDependency.cmake)
opheap_require_core()
opheap_require_json()

add_library(opheap-cli-tool INTERFACE)
add_library(opheap::cli ALIAS opheap-cli-tool)

target_compile_features(opheap-cli-tool INTERFACE cxx_std_23)
target_include_directories(opheap-cli-tool
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(opheap-cli-tool INTERFACE opheap::opheap opheap::json)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS opheap-cli-tool EXPORT opheapCliTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT opheapCliTargets NAMESPACE opheap:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-cli)
configure_package_config_file(
    cmake/opheap-cliConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-cliConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-cli
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-cliConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-cliConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-cliConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-cli
)
