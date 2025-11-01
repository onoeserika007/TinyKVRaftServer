add_executable(fiber_test basic_fiber_test.cpp)
target_link_libraries(fiber_test fiber_lib)
target_include_directories(fiber_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 无锁WaitQueue测试
add_executable(lockfree_wait_queue_test lockfree_wait_queue_test.cpp)
target_link_libraries(lockfree_wait_queue_test fiber_lib)
target_include_directories(lockfree_wait_queue_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Channel通信测试
add_executable(channel_test channel_test.cpp)
target_link_libraries(channel_test fiber_lib)
target_include_directories(channel_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 同步原语测试
add_executable(sync_test sync_test.cpp)
target_link_libraries(sync_test fiber_lib)
target_include_directories(sync_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 同步原语基础测试
add_executable(sync_primitives_test sync_primitives_test.cpp)
target_link_libraries(sync_primitives_test fiber_lib)
target_include_directories(sync_primitives_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 定时器测试
add_executable(timer_test timer_test.cpp)
target_link_libraries(timer_test fiber_lib)
target_include_directories(timer_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)