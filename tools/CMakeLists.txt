cmake_minimum_required(VERSION 3.10)

# 收集源文件
file(GLOB_RECURSE TOOLS_SRC CONFIGURE_DEPENDS *.cpp)

# 创建工具库
add_library(tools_lib STATIC ${TOOLS_SRC})
target_include_directories(tools_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(tools_lib PRIVATE 
    configs
    isa_blockisa
    common::lib
    third_party
    model_lib
)
