cmake_minimum_required(VERSION 3.10)

# 收集源文件
# file(GLOB_RECURSE RUNTIME_SRC CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE RUNTIME_SRC CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/runtime/model_rt/*.cpp
)

# 创建运行时库
add_library(runtime_lib STATIC ${RUNTIME_SRC})
target_include_directories(runtime_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(runtime_lib PRIVATE
    third_party
)
