cmake_minimum_required(VERSION 3.24)
project(opheap VERSION 0.2.0 LANGUAGES CXX)

option(OPHEAP_ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)

add_library(opheap INTERFACE)
add_library(opheap::opheap ALIAS opheap)

target_compile_features(opheap INTERFACE cxx_std_23)
target_include_directories(opheap
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
if(OPHEAP_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(opheap INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(opheap INTERFACE -fsanitize=address,undefined)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS opheap EXPORT opheapTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} PATTERN "testing" EXCLUDE)
install(EXPORT opheapTargets NAMESPACE opheap:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap)
configure_package_config_file(
    cmake/opheapConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/opheapConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap
)
# SameMajorVersion: an installed opheap accepts a find_package() request for any
# version <= its own within the same major, so installing a newer opheap here
# transparently supersedes an older system install for standalone app builds.
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/opheapConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/opheapConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/opheapConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opheap
)