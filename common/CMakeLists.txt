cmake_minimum_required(VERSION 3.10)

# 保持原有的接口库（用于头文件）
add_library(common_headers INTERFACE)
target_include_directories(common_headers INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 检查是否有 .cpp 文件，如果有则创建实现库
file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS "*.cpp")
if(COMMON_SOURCES)
    # 创建实现库
    add_library(common_impl STATIC ${COMMON_SOURCES})
    target_include_directories(common_impl PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # 创建一个组合库，包含头文件和实现
    add_library(common INTERFACE)
    target_link_libraries(common INTERFACE common_headers common_impl third_party)
    
    # 设置别名
    add_library(common::impl ALIAS common_impl)
else()
    # 如果没有实现文件，common 就是 common_headers
    add_library(common INTERFACE)
    target_link_libraries(common INTERFACE common_headers)
endif()

# 设置主要别名
add_library(common::headers ALIAS common_headers)
add_library(common::lib ALIAS common)
