add_executable(opheap_basic basic.cpp)
target_link_libraries(opheap_basic PRIVATE opheap::opheap)

add_executable(opheap_monitor monitor.cpp)
target_link_libraries(opheap_monitor PRIVATE opheap::opheap)