function(setup_soc_libraries)
    message(STATUS "Setting up P688 SOC libraries...")
    
    if(GRAPHFLOW_X86_SUPPORT)
        message(STATUS "x86 support detected, configuring SOC libraries")
            
        set(SOC_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/model/soc/lib)
        message(STATUS "SOC P688 library directory: ${SOC_LIB_DIR}")
        # 检查 SOC 库目录是否存在
        if(NOT EXISTS ${SOC_LIB_DIR})
            message(FATAL_ERROR "SOC library directory does not exist: ${SOC_LIB_DIR}")
        endif()
        
        # 检查压缩文件是否存在
        set(SOC_ARCHIVE ${SOC_LIB_DIR}/soc_libs.tar.gz)
        if(NOT EXISTS ${SOC_ARCHIVE})
            message(FATAL_ERROR "SOC library archive not found: ${SOC_ARCHIVE}")
        endif()
        
        # 检查是否已经解压
        set(EXTRACT_MARKER ${SOC_LIB_DIR}/.extracted)
        if(EXISTS ${EXTRACT_MARKER})
            message(STATUS "SOC libraries already extracted")
        else()
            message(STATUS "SOC libraries need extraction")
            
            # 解压 SOC 库
            add_custom_command(
                OUTPUT ${EXTRACT_MARKER}
                COMMAND ${CMAKE_COMMAND} -E tar xzf ${SOC_ARCHIVE}
                COMMAND ${CMAKE_COMMAND} -E touch ${EXTRACT_MARKER}
                WORKING_DIRECTORY ${SOC_LIB_DIR}
                COMMENT "Extracting SOC libraries from ${SOC_ARCHIVE}"
                DEPENDS ${SOC_ARCHIVE}
            )
        endif()
        
        # 定义提取目标
        add_custom_target(extract_soc_libs ALL
            DEPENDS ${EXTRACT_MARKER}
            COMMENT "SOC libraries extraction target"
        )
        message(STATUS "Added custom target for extracting SOC libraries: extract_soc_libs")
        
        # 查找所有 SOC 库
        message(STATUS "Looking for SOC libraries in ${SOC_LIB_DIR}")
        
        set(SOC_LIBRARIES)
        set(SOC_LIB_NAMES
            gmp
            carbon5
            AddressAgent
            AddrIntlv
            ChiRingFabric
            core_pl
            datastructure
            ddr5sim
            dpusim
            hbmsim
            HHA_ACC
            GENERIC_CHI_IF
            GenericSingleCore_SingleCore
            isa
            L3_DAT_RAM
            L3TAG
            L4DAT
            ParallelScheduler
            PA
            PCIE
            POE
            SLLC
            SoC_MP
            system
            tracemanager
            TrafGenOnLine
        )
        
        foreach(lib_name IN LISTS SOC_LIB_NAMES)
            if(lib_name STREQUAL "gmp")
                find_library(${lib_name}_lib 
                    NAMES gmp libgmp.so.3
                    PATHS ${SOC_LIB_DIR} 
                    NO_DEFAULT_PATH
                    NO_CMAKE_FIND_ROOT_PATH
                    NO_SYSTEM_ENVIRONMENT_PATH
                )
            else()
                find_library(${lib_name}_lib ${lib_name} 
                    PATHS ${SOC_LIB_DIR}
                    NO_DEFAULT_PATH
                    NO_CMAKE_FIND_ROOT_PATH
                    NO_SYSTEM_ENVIRONMENT_PATH
                )
            endif()
            
            if(${lib_name}_lib)
                message(STATUS "Found SOC library: ${lib_name} -> ${${lib_name}_lib}")
                list(APPEND SOC_LIBRARIES ${${lib_name}_lib})
            else()
                message(WARNING "SOC library not found: ${lib_name}")
            endif()
        endforeach()
        
        # 检查是否找到了任何库
        list(LENGTH SOC_LIBRARIES SOC_LIBRARIES_COUNT)
        if(SOC_LIBRARIES_COUNT EQUAL 0)
            message(FATAL_ERROR "No SOC libraries found in ${SOC_LIB_DIR}")
        else()
            message(STATUS "Found ${SOC_LIBRARIES_COUNT} SOC libraries")
        endif()
        
        # 创建处理 SOC 库链接的接口库
        add_library(soc_libraries_interface INTERFACE)
        
        # 设置 SOC 库的链接选项
        target_link_directories(soc_libraries_interface INTERFACE ${SOC_LIB_DIR})
        
        # 为每个 SOC 库设置正确的链接选项
        foreach(soc_lib ${SOC_LIBRARIES})
            if(soc_lib MATCHES ".*libgmp\\.so.*")
                # 对于 gmp 库，使用绝对路径和特殊链接选项避免冲突
                target_link_libraries(soc_libraries_interface INTERFACE
                    "-Wl,--as-needed"
                    "${soc_lib}"
                    "-Wl,--no-as-needed"
                )
                message(STATUS "Configured GMP library with conflict avoidance: ${soc_lib}")
            else()
                # 对于其他库，正常链接
                target_link_libraries(soc_libraries_interface INTERFACE "${soc_lib}")
            endif()
        endforeach()
        
        # 设置运行时库搜索路径
        target_link_options(soc_libraries_interface INTERFACE
            "-Wl,-rpath,${SOC_LIB_DIR}"
            "-Wl,--disable-new-dtags"
            # 让链接器自动解决依赖
            "-Wl,--allow-shlib-undefined"
        )
        
        # 添加编译定义
        target_compile_definitions(soc_libraries_interface INTERFACE -DEXT_SOC_INTF)
        
        # 导出变量到父作用域
        set(SOC_LIB_NAMES_GLOBAL ${SOC_LIB_NAMES} PARENT_SCOPE)
        set(SOC_LIBS_GLOBAL ${SOC_LIBRARIES} PARENT_SCOPE)
        set(SOC_EXTRACT_TARGET_GLOBAL extract_soc_libs PARENT_SCOPE)
        set(SOC_LIBRARIES_INTERFACE soc_libraries_interface PARENT_SCOPE)
        
        message(STATUS "SOC setup complete with interface library")
        
    else()
        message(STATUS "x86 support not detected, skipping SOC libraries setup")
    endif()
endfunction()

# 辅助函数：链接 SOC 库到目标
function(link_soc_libraries TARGET_NAME)
    if(GRAPHFLOW_X86_SUPPORT AND TARGET ${TARGET_NAME} AND TARGET soc_libraries_interface)
        message(STATUS "Linking SOC libraries to ${TARGET_NAME}")
        
        # 添加依赖关系
        add_dependencies(${TARGET_NAME} ${SOC_EXTRACT_TARGET_GLOBAL})
        
        # 链接接口库
        target_link_libraries(${TARGET_NAME} PRIVATE soc_libraries_interface)
        
        message(STATUS "Suaccelssfully linked SOC libraries to ${TARGET_NAME}")
    elseif(GRAPHFLOW_X86_SUPPORT AND TARGET ${TARGET_NAME})
        message(WARNING "SOC support enabled but SOC interface library not available for ${TARGET_NAME}")
    endif()
endfunction()
