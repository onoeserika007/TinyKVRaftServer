cmake_minimum_required(VERSION 3.22)

# 收集源文件
file(GLOB RAFT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# 创建raft库
add_library(raft_lib STATIC ${RAFT_SOURCES})

# 设置包含目录
target_include_directories(raft_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/fiber/include
    ${PROJECT_SOURCE_DIR}/third_party/basic_libs/include
)

# 链接依赖库
target_link_libraries(raft_lib
    fiber_lib
    base_lib
)

# 设置输出目录
set_target_properties(raft_lib PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib
)
