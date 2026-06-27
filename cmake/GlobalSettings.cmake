set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SOC 设置
option(GENERIC_SOC "Use generic SOC V3.1.1 implementation (default: OFF)" OFF)
option(GENERIC_SOC_NEW "Use new generic SOC V6.6.4 implementation (default: OFF)" OFF)

option(BUILD_TESTS "Build unit tests" OFF)
option(DISABLE_DEBUG_SYMBOLS "Disable debug symbols" OFF)
option(CMAKE_VERBOSE_MAKEFILE "Enable verbose" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer (and LeakSanitizer)" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

set(COVERAGE_MODE "FULL" CACHE STRING "Coverage mode: FULL or DIFF")
set_property(CACHE COVERAGE_MODE PROPERTY STRINGS FULL DIFF)

set(COVERAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/coverage" CACHE PATH "Coverage data/output directory")

# --- 字符串配置 (使用 set CACHE) ---
# 逻辑：如果命令行没传 -DOPT_LEVEL，且缓存里也没有，则默认为 O2
# 如果命令行传了 -DOPT_LEVEL=O3，则使用 O3
# 如果命令行传了 -DOPT_LEVEL= (空)，则下面的逻辑会将其重置为 O2
if(NOT DEFINED OPT_LEVEL)
    set(OPT_LEVEL "O2" CACHE STRING "Compilation optimization level (O0, O2, O3, Os)")
endif()
# 容错处理：如果用户显式传入空字符串，强制改回 O2
if("${OPT_LEVEL}" STREQUAL "")
    set(OPT_LEVEL "O2" CACHE STRING "Compilation optimization level" FORCE)
endif()

# CCache 配置
find_program(CCACHE_EXECUTABLE ccache)
if(CCACHE_EXECUTABLE)
    set(ccacheEnv CCACHE_SLOPPINESS=pch_defines,time_macros)
    foreach(lang IN ITEMS C CXX)
        set(CMAKE_${lang}_COMPILER_LAUNCHER ${CMAKE_COMMAND} -E env ${ccacheEnv} ${CCACHE_EXECUTABLE})
    endforeach()
endif()

function(gf_add_compile_definitions)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
        add_compile_definitions(${ARGN})
    else()
        foreach(definition IN LISTS ARGN)
            add_definitions("-D${definition}")
        endforeach()
    endif()
endfunction()

macro(gf_add_link_options)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
        add_link_options(${ARGN})
    else()
        foreach(link_option IN ITEMS ${ARGN})
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${link_option}")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${link_option}")
            set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${link_option}")
        endforeach()
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}" PARENT_SCOPE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}" PARENT_SCOPE)
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}" PARENT_SCOPE)
    endif()
endmacro()

# 平台检测
function(gf_detect_platform)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        message(STATUS "Detected x86-64 system")
        set(GRAPHFLOW_X86_SUPPORT ON PARENT_SCOPE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        message(STATUS "Detected AArch64 system")
        set(GRAPHFLOW_ARM_SUPPORT ON PARENT_SCOPE)
    endif()
endfunction()

# 全局编译定义
gf_add_compile_definitions("PACKAGE=blockisa" "PACKAGE_VERSION=0.20")


# 设置全局编译器选项
function(gf_set_global_compiler_options)
    # 打印当前配置，方便调试
    message(STATUS ">>> Build Configuration:")
    message(STATUS "    Optimization Level: ${OPT_LEVEL}")
    message(STATUS "    AddressSanitizer  : ${ENABLE_ASAN}")
    message(STATUS "    UBSanitizer       : ${ENABLE_UBSAN}")
    # ------------------------------------------------------------------------
    # 1. 基础编译选项
    # ------------------------------------------------------------------------
    add_compile_options(
        -Wall
        -Werror
    )
    if(NOT DISABLE_DEBUG_SYMBOLS)
        add_compile_options(-g)
    endif()
    # 应用优化级别 (注意变量前加 -)
    # 如果 OPT_LEVEL 是 "O2"，这里会变成 "-O2"
    add_compile_options(-${OPT_LEVEL})
    # ------------------------------------------------------------------------
    # 2. 基础链接选项
    # ------------------------------------------------------------------------
    if(NOT DISABLE_DEBUG_SYMBOLS)
        gf_add_link_options(-g)
    endif()
    # -lm 仅在 Linux/Unix 下需要
    if(UNIX AND NOT APPLE)
        gf_add_link_options(-lm)
    endif()
    # ------------------------------------------------------------------------
    # 3. Sanitizer 配置 (根据外部传入的 option 决定)
    # ------------------------------------------------------------------------
    if(ENABLE_ASAN)
        message(STATUS "    [ASAN] AddressSanitizer is ENABLED")

        # 编译和链接都需要加
        add_compile_options(
            -fsanitize=address
            -fno-omit-frame-pointer
        )
        gf_add_link_options(
            -fsanitize=address
            -fno-omit-frame-pointer
        )
    endif()
    if(ENABLE_UBSAN)
        message(STATUS "    [UBSAN] UndefinedBehaviorSanitizer is ENABLED")
        add_compile_options(-fsanitize=undefined)
        gf_add_link_options(-fsanitize=undefined)
    endif()

    # 如果同时开了 ASAN 和 UBSAN，GCC/Clang 通常支持同时链接
    # 但建议主要调试时只开一个，避免干扰

    # ------------------------------------------------------------------------
    # 4. 代码覆盖率检查
    # ------------------------------------------------------------------------
    if(ENABLE_COVERAGE)
    if(NOT COVERAGE_MODE STREQUAL "FULL" AND NOT COVERAGE_MODE STREQUAL "DIFF")
        message(FATAL_ERROR "Invalid COVERAGE_MODE='${COVERAGE_MODE}', expected FULL or DIFF")
    endif()
    message(STATUS "    [COVERAGE] Code coverage is ENABLED")
    message(STATUS "    [COVERAGE] Mode       : ${COVERAGE_MODE}")
    message(STATUS "    [COVERAGE] Output Dir : ${COVERAGE_OUTPUT_DIR}")
    add_compile_options(--coverage)
    gf_add_link_options(--coverage)
    gf_add_compile_definitions(
        GF_ENABLE_COVERAGE=1
        GF_COVERAGE_MODE="${COVERAGE_MODE}"
        GF_COVERAGE_OUTPUT_DIR="${COVERAGE_OUTPUT_DIR}"
    )
endif()
endfunction()
