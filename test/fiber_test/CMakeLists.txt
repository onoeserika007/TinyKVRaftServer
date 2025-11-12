# Fiber vs Thread IO 性能对比测试
add_executable(fiber_vs_thread_io_test fiber_vs_thread_io_test.cpp)
target_link_libraries(fiber_vs_thread_io_test fiber_lib gtest gtest_main pthread)
target_include_directories(fiber_vs_thread_io_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
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

# IO集成测试
add_executable(io_test io_test.cpp)
target_link_libraries(io_test fiber_lib)
target_include_directories(io_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(mutex_test mutex_test.cpp)
target_link_libraries(mutex_test fiber_lib)
target_include_directories(mutex_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(mutex_simple_test mutex_simple_test.cpp)
target_link_libraries(mutex_simple_test fiber_lib)
target_include_directories(mutex_simple_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)