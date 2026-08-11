cmake_minimum_required(VERSION 3.24)
project(opheap-module-cli-tool VERSION 0.1.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/OpheapDependency.cmake)
opheap_require_core()
opheap_require_json()

add_library(opheap-module-cli-tool INTERFACE)
add_library(opheap::module::cli ALIAS opheap-module-cli-tool)

target_compile_features(opheap-module-cli-tool INTERFACE cxx_std_23)
target_include_directories(opheap-module-cli-tool
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(opheap-module-cli-tool INTERFACE opheap::opheap opheap::json)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS opheap-module-cli-tool EXPORT opheapModuleCliTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} PATTERN "testing" EXCLUDE)
install(EXPORT opheapModuleCliTargets NAMESPACE opheap:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-module-cli)
configure_package_config_file(
    cmake/opheap-module-cliConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-module-cliConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-module-cli
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-module-cliConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-module-cliConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-module-cliConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-module-cli
)
