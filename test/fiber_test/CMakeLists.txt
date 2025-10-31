add_executable(fiber_test fiber_test.cpp)
target_link_libraries(fiber_test fiber_lib)
target_include_directories(fiber_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Scheduler Basic Test
add_executable(scheduler_test scheduler_test.cpp)
target_link_libraries(scheduler_test fiber_lib)
target_include_directories(scheduler_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 用户友好接口测试
add_executable(user_interface_test user_interface_test.cpp)
target_link_libraries(user_interface_test fiber_lib)
target_include_directories(user_interface_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Lua语义测试
add_executable(lua_style_test lua_style_test.cpp)
target_link_libraries(lua_style_test fiber_lib)
target_include_directories(lua_style_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Go语义测试
add_executable(go_style_test go_style_test.cpp)
target_link_libraries(go_style_test fiber_lib)
target_include_directories(go_style_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 并发Go语义测试
add_executable(concurrent_go_test concurrent_go_test.cpp)
target_link_libraries(concurrent_go_test fiber_lib pthread)
target_include_directories(concurrent_go_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)