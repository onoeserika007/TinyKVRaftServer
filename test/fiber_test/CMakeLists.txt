add_executable(fiber_test fiber_test.cpp)
target_link_libraries(fiber_test fiber_lib)
target_include_directories(fiber_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)