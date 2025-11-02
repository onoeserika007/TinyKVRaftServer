# 获取所有的测试源文件
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# 为每个测试源文件创建可执行文件
foreach(TEST_SOURCE ${TEST_SOURCES})
    # 获取文件名（不含扩展名）
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    # 创建可执行文件
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    # 链接库
    target_link_libraries(${TEST_NAME} 
        rpc_lib 
        fiber_lib
        base_lib
    )
    
    # 设置包含目录
    target_include_directories(${TEST_NAME} PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/rpc/include
        ${PROJECT_SOURCE_DIR}/src/fiber/include
        ${PROJECT_SOURCE_DIR}/third_party/basic_libs/include
    )
endforeach()
