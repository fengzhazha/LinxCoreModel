function(filter_generated_files INPUT_LIST OUTPUT_LIST)
    set(FILTERED_LIST)
    foreach(item ${INPUT_LIST})
        # 检查是否是生成的文件（以_config.cpp结尾）
        string(REGEX MATCH "_config\\.cpp$" IS_GENERATED ${item})
        if(NOT IS_GENERATED)
            list(APPEND FILTERED_LIST ${item})
        endif()
    endforeach()
    set(${OUTPUT_LIST} ${FILTERED_LIST} PARENT_SCOPE)
endfunction()

# configs/CMakeLists.txt
cmake_minimum_required(VERSION 3.0)

# 设置gen_configs.py路径
set(GEN_CONFIGS_SCRIPT ${CMAKE_SOURCE_DIR}/tools/gen_configs.py)

# 查找TOML文件
file(GLOB TOML_FILES CONFIGURE_DEPENDS "*.toml")

# 创建生成文件列表
set(GENERATED_FILES)
foreach(toml ${TOML_FILES})
    get_filename_component(BASE_NAME ${toml} NAME_WE)
    string(TOLOWER ${BASE_NAME} LOWERCASE_NAME)
    list(APPEND GENERATED_FILES 
         ${CMAKE_CURRENT_SOURCE_DIR}/${LOWERCASE_NAME}_config.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/${LOWERCASE_NAME}_config.h)
endforeach()

# 添加生成命令
add_custom_command(
    OUTPUT ${GENERATED_FILES}
    COMMAND ${CMAKE_COMMAND} -E echo
          "[configs] generating in: ${CMAKE_CURRENT_SOURCE_DIR}, bin: ${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND ${PYTHON_EXECUTABLE} ${GEN_CONFIGS_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${TOML_FILES} ${GEN_CONFIGS_SCRIPT}
    COMMENT "Generating config files"
    VERBATIM
)

# 扫描所有cpp文件，然后过滤掉生成的
file(GLOB_RECURSE ALL_CPP_FILES CONFIGURE_DEPENDS *.cpp)
filter_generated_files("${ALL_CPP_FILES}" EXISTING_SRC_FILES)

# 创建库目标
add_library(configs STATIC 
    ${EXISTING_SRC_FILES}
    ${GENERATED_FILES}
)

target_include_directories(configs PUBLIC .)
