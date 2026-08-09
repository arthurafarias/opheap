add_executable(opheap_basic basic.cpp)
target_link_libraries(opheap_basic PRIVATE opheap::opheap)

add_subdirectory(beast_credentials)
