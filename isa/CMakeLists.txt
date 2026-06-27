cmake_minimum_required(VERSION 3.10)

set(CODEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/codec)
# 设置生成文件的目录
set(GENERATED_DECODE_DIR ${CODEC_DIR}/generatedfiles)
file(MAKE_DIRECTORY ${GENERATED_DECODE_DIR})

# 定义生成的文件列表
set(GENERATED_FILE_LIST
    ${GENERATED_DECODE_DIR}/decode-inst16.cpp
    ${GENERATED_DECODE_DIR}/decode-inst32.cpp
    ${GENERATED_DECODE_DIR}/decode-inst48.cpp
    ${GENERATED_DECODE_DIR}/decode-inst64.cpp
    ${GENERATED_DECODE_DIR}/encode-inst16.cpp
    ${GENERATED_DECODE_DIR}/encode-inst32.cpp
    ${GENERATED_DECODE_DIR}/encode-inst64.cpp
)

# 自定义命令生成极
add_custom_command(
    OUTPUT ${GENERATED_FILE_LIST}
    COMMAND ./build.sh
    WORKING_DIRECTORY ${CODEC_DIR}
    COMMENT "Generating decoding dependency files via build.sh"
    DEPENDS
        ${CODEC_DIR}/build.sh
        ${CODEC_DIR}/decodetree.py
        ${CODEC_DIR}/encodetree.py
        ${CODEC_DIR}/decodefiles/block16.decode
        ${CODEC_DIR}/decodefiles/block32.decode
        ${CODEC_DIR}/decodefiles/block48.decode
        ${CODEC_DIR}/decodefiles/block64.decode
    VERBATIM
)

set(CODEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/codec)
# 设置生成文件的目录
set(GENERATED_DECODE_DIR ${CODEC_DIR}/generatedfiles)
file(MAKE_DIRECTORY ${GENERATED_DECODE_DIR})

# 定义生成的文件列表
set(GENERATED_FILE_LIST
    ${GENERATED_DECODE_DIR}/decode-inst16.cpp
    ${GENERATED_DECODE_DIR}/decode-inst32.cpp
    ${GENERATED_DECODE_DIR}/decode-inst48.cpp
    ${GENERATED_DECODE_DIR}/decode-inst64.cpp
    ${GENERATED_DECODE_DIR}/encode-inst16.cpp
    ${GENERATED_DECODE_DIR}/encode-inst32.cpp
    ${GENERATED_DECODE_DIR}/encode-inst64.cpp
)

# 自定义命令生成极
add_custom_command(
    OUTPUT ${GENERATED_FILE_LIST}
    COMMAND ./build.sh
    WORKING_DIRECTORY ${CODEC_DIR}
    COMMENT "Generating decoding dependency files via build.sh"
    DEPENDS
        ${CODEC_DIR}/build.sh
        ${CODEC_DIR}/decodetree.py
        ${CODEC_DIR}/decodefiles/block16.decode
        ${CODEC_DIR}/decodefiles/block32.decode
        ${CODEC_DIR}/decodefiles/block48.decode
        ${CODEC_DIR}/decodefiles/block64.decode
    VERBATIM
)

# 使用 GLOB 收集源文件 - 只处理本目录下的文件
file(GLOB_RECURSE ISA_SRC CONFIGURE_DEPENDS *.cpp)
# 从 GLOB 结果中排除特定的生成文件
list(FILTER ISA_SRC EXCLUDE REGEX ".*decode-.*\\.cpp$")
list(FILTER ISA_SRC EXCLUDE REGEX ".*encode-.*\\.cpp$")

# 创建 ISA 库
add_library(isa_blockisa STATIC ${ISA_SRC} ${GENERATED_FILE_LIST})

set(CODEC_COMPILED_SRCS ${GENERATED_FILE_LIST})
foreach(src ${CODEC_COMPILED_SRCS})
  set_source_files_properties(${src} PROPERTIES
    COMPILE_OPTIONS
      "$<$<CXX_COMPILER_ID:GNU,Clang>:-w>;$<$<CXX_COMPILER_ID:MSVC>:/W0>"
  )
endforeach()

target_include_directories(isa_blockisa PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/softfloat/include
)

# 链接生成的文件
target_link_libraries(isa_blockisa PUBLIC
    softfloat
)
# 使用 PUBLIC 链接 common::lib 以确保包含目录被继承
target_link_libraries(isa_blockisa PUBLIC
    common::lib
)
