cmake_minimum_required(VERSION 3.10.2)

project(softfloat)

set (CMAKE_CXX_STANDARD 14)
set(CMAKE_C_FLAGS "-Werror-implicit-function-declaration")

include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

file(GLOB FPU RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    "fpu/*.c")

add_library(softfloat STATIC ${FPU})

# 为 softfloat 设置宽松的编译选项
target_compile_options(softfloat PRIVATE
    -w  # 禁用所有警告
)

# 根据编译器类型添加特定的宽松选项
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(softfloat PRIVATE
        -Wno-error
        -Wno-pedantic
        -Wno-extra
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-implicit-fallthrough
        -fno-strict-aliasing
    )
endif()

# 设置宽松的编译器标准
set_target_properties(softfloat PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED OFF
    C_EXTENSIONS ON
)

add_executable(example "${CMAKE_CURRENT_LIST_DIR}/example/example.cpp")
target_link_libraries(example PUBLIC softfloat)