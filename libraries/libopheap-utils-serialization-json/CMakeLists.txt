cmake_minimum_required(VERSION 3.24)
project(opheap-utils-serialization-json VERSION 0.1.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/OpheapDependency.cmake)
opheap_require_core()

add_library(opheap-utils-serialization-json INTERFACE)
add_library(opheap::json ALIAS opheap-utils-serialization-json)

target_compile_features(opheap-utils-serialization-json INTERFACE cxx_std_23)
target_include_directories(opheap-utils-serialization-json
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(opheap-utils-serialization-json INTERFACE opheap::opheap)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS opheap-utils-serialization-json EXPORT opheapUtilsSerializationJsonTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT opheapUtilsSerializationJsonTargets NAMESPACE opheap::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-utils-serialization-json)
configure_package_config_file(
    cmake/opheap-utils-serialization-jsonConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-utils-serialization-jsonConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-utils-serialization-json
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-utils-serialization-jsonConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-utils-serialization-jsonConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/opheap-utils-serialization-jsonConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap-utils-serialization-json
)
