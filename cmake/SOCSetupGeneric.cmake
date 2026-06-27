function(setup_generic_soc_libraries)
    message(STATUS "Setting up GENERIC SOC libraries...")
    
    if(GRAPHFLOW_X86_SUPPORT)
        message(STATUS "x86 support detected, configuring SOC libraries")
        set(SOC_GENERIC_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/model/generic_soc/lib")
        message(STATUS "SOC GENERIC library directory: ${SOC_GENERIC_LIB_DIR}")
        # 检查 SOC 库目录是否存在
        if(NOT EXISTS "${SOC_GENERIC_LIB_DIR}")
            message(FATAL_ERROR "SOC library directory does not exist: ${SOC_GENERIC_LIB_DIR}")
        endif()
        
        # 检查压缩文件是否存在
        set(SOC_ARCHIVE "${SOC_GENERIC_LIB_DIR}/soc_libs.tar.gz")
        if(NOT EXISTS "${SOC_ARCHIVE}")
            message(FATAL_ERROR "SOC library archive not found: ${SOC_ARCHIVE}")
        endif()
        
        # 检查是否已经解压
        set(EXTRACT_MARKER "${SOC_GENERIC_LIB_DIR}/.extracted")
        if(EXISTS "${EXTRACT_MARKER}")
            message(STATUS "SOC libraries already extracted")
        else()
            message(STATUS "SOC libraries need extraction")
            
            # 解压 SOC 库
            add_custom_command(
                OUTPUT "${EXTRACT_MARKER}"
                COMMAND "${CMAKE_COMMAND}" -E tar xzf "${SOC_ARCHIVE}"
                COMMAND "${CMAKE_COMMAND}" -E touch "${EXTRACT_MARKER}"
                WORKING_DIRECTORY "${SOC_GENERIC_LIB_DIR}"
                COMMENT "Extracting SOC libraries from ${SOC_ARCHIVE}"
                DEPENDS "${SOC_ARCHIVE}"
            )
        endif()

        # 判断export_generic_mp_generic和parameter_0链接是否可用，是否删除旧的软链接重新建立链接
        set(TARGET_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        set(MODEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/model/generic_soc/export_generic_mp_generic")
        message(STATUS "Judge soft link file from ${TARGET_DIR}/export_generic_mp_generic to  ${MODEL_DIR}")
        set(cur_target "${CMAKE_CURRENT_SOURCE_DIR}")
        if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.14)
            if((EXISTS "${TARGET_DIR}/export_generic_mp_generic") AND (EXISTS "${TARGET_DIR}/parameter_0")) # AND  IS_SYMLINK "${TARGET_DIR}/export_generic_mp_generic"
                file(READ_SYMLINK "${TARGET_DIR}/export_generic_mp_generic" cur_target)
            endif()
        endif()
        message(STATUS "Judge soft link file STREQUAL ${MODEL_DIR} to ${cur_target}")
        if(NOT "${cur_target}" STREQUAL "${MODEL_DIR}")
            message(STATUS "Symlink Donot STRE QUAL old soft link: ${TARGET_DIR}/export_generic_mp_generic -> ${cur_target}")
            message(STATUS "Symlink STREQUAL old soft link file target: Should change to set ${MODEL_DIR}.")
            file(REMOVE_RECURSE "${TARGET_DIR}/export_generic_mp_generic" "${TARGET_DIR}/parameter_0")
            execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "export_generic_mp_generic/parameter_0" "${TARGET_DIR}/parameter_0")
            execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "${MODEL_DIR}" "${TARGET_DIR}/export_generic_mp_generic")
        else()
            message(STATUS "link STRE QUAL reserve old soft link: ${TARGET_DIR}/export_generic_mp_generic -> ${cur_target}.")
            message(STATUS "Symlink STREQUAL old soft link file target: Should set ${MODEL_DIR}.")
        endif()

        # 定义提取目标
        add_custom_target(extract_generic_soc_libs ALL
            DEPENDS "${EXTRACT_MARKER}"
            COMMENT "SOC libraries extraction target"
        )
        message(STATUS "Added custom target for extracting SOC libraries: extract_generic_soc_libs")
        
        # 查找所有 SOC 库
        message(STATUS "Looking for generic SOC libraries in ${SOC_GENERIC_LIB_DIR}")
      
        set(GENERIC_SOC_LIBRARIES)
        ## use for GENERIC  (systemc-2.3.1)
        set(SOC_GENERIC_LIB_NAMES
            AA AddrIntlv Afifo aicpu_wrapper pem_accelerator aicpu AXI_STREAM_BUS ChiRingFabric common LpddrWrap lpddrsim
            CoreAdapter CoreL3Wrapper DDR_Inf dramsim DSA DVPP_CA EslTop Global GENERIC_CHI_IF  L2Buf L3TAG L4DAT Log MATA mcu_wrapper mcu_loop
            MemoryModule model_api NcMpi NET_FASTMODEL NETWORK_ICL NIC ParallelScheduler  PMU POE CuberWrapper  stars stars_wrapper
            PowerModel qtest_api RDMA SCHE SDMAA SDMAM SLLC SMMU SoC systemc-2.3.1 GenericCoreCluster TaskSched TgWrapper UB Utility zptwrapper
        )

        # use for generic : model/generic_soc/lib
        foreach(lib_generic_name IN LISTS SOC_GENERIC_LIB_NAMES)
            if(lib_name STREQUAL "boost_serialization")
                find_library(${lib_name}_lib 
                    NAMES boost_serialization  
                    PATHS "${SOC_GENERIC_LIB_DIR}"
                    NO_DEFAULT_PATH
                    NO_CMAKE_FIND_ROOT_PATH
                    NO_SYSTEM_ENVIRONMENT_PATH
                )
            else()
                find_library(${lib_generic_name}_lib ${lib_generic_name} 
                    PATHS "${SOC_GENERIC_LIB_DIR}"
                    NO_DEFAULT_PATH
                    NO_CMAKE_FIND_ROOT_PATH
                    NO_SYSTEM_ENVIRONMENT_PATH
                )
            endif()
            if(${lib_generic_name}_lib)
                message(STATUS "Found SOC library: ${lib_generic_name} -> ${${lib_generic_name}_lib}")
                list(APPEND GENERIC_SOC_LIBRARIES ${${lib_generic_name}_lib})
            else()
                message(WARNING "SOC library not found: ${lib_generic_name}")
            endif()
        endforeach()


        # GENERIC soc任何库
        list(LENGTH GENERIC_SOC_LIBRARIES GENERIC_SOC_LIBRARIES_COUNT)
        if(GENERIC_SOC_LIBRARIES_COUNT EQUAL 0)
            message(FATAL_ERROR "No SOC libraries found in ${SOC_GENERIC_LIB_DIR}")
        else()
            message(STATUS "Found ${GENERIC_SOC_LIBRARIES_COUNT} SOC libraries")
        endif()


        # 创建处理 SOC 库链接的接口库
        add_library(soc_generic_libraries_interface INTERFACE)

        # 设置 SOC 库的链接选项
        if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
            target_link_directories( soc_generic_libraries_interface INTERFACE "${SOC_GENERIC_LIB_DIR}")
        else()
            target_link_libraries( soc_generic_libraries_interface INTERFACE "-L${SOC_GENERIC_LIB_DIR}")
        endif()
        
        # 为每个 SOC 库设置正确的链接选项
        foreach(soc_lib ${GENERIC_SOC_LIBRARIES})
            if(soc_lib MATCHES ".*libgmp\\.so.*")
                # 对于 gmp 库，使用绝对路径和特殊链接选项避免冲突
                target_link_libraries( soc_generic_libraries_interface INTERFACE
                    "-Wl,--as-needed"
                    "${soc_lib}"
                    "-Wl,--no-as-needed"
                )
                message(STATUS "Configured GMP library with conflict avoidance: ${soc_lib}")
            elseif(soc_lib MATCHES ".*libboost_serialization\\.so.*")
                # 对于 boost_serialization 库，使用绝对路径和特殊链接选项避免冲突
                target_link_libraries( soc_generic_libraries_interface INTERFACE
                    "-Wl,--as-needed"
                    "${soc_lib}"
                    "-Wl,--no-as-needed"
                )
                message(STATUS "Configured boost_serialization library with conflict avoidance: ${soc_lib}")

            else()
                # 对于其他库，正常链接
                target_link_libraries( soc_generic_libraries_interface INTERFACE "${soc_lib}")
            endif()
        endforeach()
        
        # 设置运行时库搜索路径
        if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
            target_link_options( soc_generic_libraries_interface INTERFACE
                "-Wl,-rpath,${SOC_GENERIC_LIB_DIR}"
                "-Wl,--disable-new-dtags"
                # 让链接器自动解决依赖
                "-Wl,--allow-shlib-undefined"
            )
        else()
            target_link_libraries( soc_generic_libraries_interface INTERFACE
                "-Wl,-rpath,${SOC_GENERIC_LIB_DIR}"
                "-Wl,--disable-new-dtags"
                "-Wl,--allow-shlib-undefined"
            )
        endif()
        
        # 添加编译定义
        target_compile_definitions( soc_generic_libraries_interface INTERFACE -DEXT_SOC_INTF)
        
        # 导出变量到父作用域
        set(SOC_GENERIC_LIB_NAMES_GLOBAL ${SOC_GENERIC_LIB_NAMES} PARENT_SCOPE)
        set(SOC_GENERIC_LIBS_GLOBAL ${GENERIC_SOC_LIBRARIES} PARENT_SCOPE)
        set(SOC_GENERIC_EXTRACT_TARGET_GLOBAL extract_generic_soc_libs PARENT_SCOPE)
        set(SOC_GENERIC_LIBRARIES_INTERFACE soc_generic_libraries_interface PARENT_SCOPE)
        
        message(STATUS "SOC setup complete with interface library")
        
    else()
        message(STATUS "x86 support not detected, skipping generic SOC libraries setup")
    endif()
endfunction()

# 辅助函数：链接 SOC 库到目标
function(link_soc_generic_libraries TARGET_NAME)
    if(GRAPHFLOW_X86_SUPPORT AND TARGET ${TARGET_NAME} AND TARGET soc_generic_libraries_interface)
        message(STATUS "Linking SOC generic libraries to ${TARGET_NAME}")
        
        # 添加依赖关系
        add_dependencies(${TARGET_NAME} ${SOC_GENERIC_EXTRACT_TARGET_GLOBAL})
        
        # 链接接口库
        target_link_libraries(${TARGET_NAME} PRIVATE soc_generic_libraries_interface)
        
        message(STATUS "Suaccelssfully linked SOC generic libraries to ${TARGET_NAME}")
    elseif(GRAPHFLOW_X86_SUPPORT AND TARGET ${TARGET_NAME})
        message(WARNING "SOC support enabled but SOC interface library not available for ${TARGET_NAME}")
    endif()
endfunction()
