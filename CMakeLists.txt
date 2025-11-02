cmake_minimum_required(VERSION 3.22)
set(CMAKE_C_COMPILER gcc-13)
set(CMAKE_CXX_COMPILER g++-13)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
project(TinyKVRaftServer)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置项目库文件搜索路径 -L
link_directories(${PROJECT_SOURCE_DIR}/lib)

# third_party libraries
add_subdirectory(third_party/basic_libs)

# src
add_subdirectory(src/fiber)
add_subdirectory(src/rpc)

# test
add_subdirectory(test/fiber_test)
add_subdirectory(test/rpc_test)
