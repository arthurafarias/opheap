# Deliberately NOT registered with add_test()/ctest: these scenarios trade the
# unit tests' sub-second runtime for real thread/iteration/payload counts, and
# are meant to be run on demand (or from a dedicated, longer CI job), not on
# every `ctest` invocation. Build with -DOPHEAP_BUILD_STRESS_TESTS=ON and run
# the resulting opheap_stress binary directly; pass --scale N for a heavier
# soak run or --help for usage.
find_package(Threads REQUIRED)

file(GLOB STRESS_SCENARIOS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/*.cpp")
add_executable(opheap_stress main.cpp ${STRESS_SCENARIOS})
target_link_libraries(opheap_stress PRIVATE opheap::opheap Threads::Threads)
target_compile_features(opheap_stress PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(opheap_stress PRIVATE /W4 /permissive-)
else()
    target_compile_options(opheap_stress PRIVATE -Wall -Wextra -Wpedantic)
endif()
