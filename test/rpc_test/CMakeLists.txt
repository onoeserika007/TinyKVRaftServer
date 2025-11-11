# 添加 googletest
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/googletest ${CMAKE_CURRENT_BINARY_DIR}/googletest)

# 获取所有的测试源文件
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# 为每个测试源文件创建可执行文件
foreach(TEST_SOURCE ${TEST_SOURCES})
    # 获取文件名（不含扩展名）
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    # 创建可执行文件
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    # encoder_test 和 rpc_test 需要 gtest
    if(TEST_NAME STREQUAL "encoder_test")
        target_link_libraries(${TEST_NAME} 
            raft_lib
            rpc_lib 
            fiber_lib
            base_lib
            gtest
        )
        target_include_directories(${TEST_NAME} PUBLIC 
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src/raft/include
            ${PROJECT_SOURCE_DIR}/src/rpc/include
            ${PROJECT_SOURCE_DIR}/src/fiber/include
            ${PROJECT_SOURCE_DIR}/third_party/basic_libs/include
            ${PROJECT_SOURCE_DIR}/third_party/pfr/include
            ${PROJECT_SOURCE_DIR}/third_party/googletest/googletest/include
        )
    elseif(TEST_NAME STREQUAL "rpc_test")
        target_link_libraries(${TEST_NAME} 
            rpc_lib 
            fiber_lib
            base_lib
            gtest
        )
        target_include_directories(${TEST_NAME} PUBLIC 
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src/rpc/include
            ${PROJECT_SOURCE_DIR}/src/fiber/include
            ${PROJECT_SOURCE_DIR}/third_party/basic_libs/include
            ${PROJECT_SOURCE_DIR}/third_party/pfr/include
            ${PROJECT_SOURCE_DIR}/third_party/googletest/googletest/include
        )
    else()
        # 其他测试
        target_link_libraries(${TEST_NAME} 
            rpc_lib 
            fiber_lib
            base_lib
        )
        target_include_directories(${TEST_NAME} PUBLIC 
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src/rpc/include
            ${PROJECT_SOURCE_DIR}/src/fiber/include
            ${PROJECT_SOURCE_DIR}/third_party/basic_libs/include
            ${PROJECT_SOURCE_DIR}/third_party/pfr/include
        )
    endif()
endforeach()
