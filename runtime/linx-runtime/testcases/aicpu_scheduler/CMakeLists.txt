cmake_minimum_required(VERSION 3.8)

include_directories(${PROJECT_SOURCE_DIR}/include)
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/data)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
file(GLOB_RECURSE ASM_SOURCES ${PROJECT_SOURCE_DIR}/testcases/*.s)

IF (NOT DEFINED RUN_CORES)
    SET(RUN_CORES 8)
ENDIF()
ADD_DEFINITIONS(-DRUN_CORES=${RUN_CORES})

IF (NOT DEFINED AI_CORES)
    SET(AI_CORES 8)
ENDIF()
ADD_DEFINITIONS(-DAI_CORES=${AI_CORES})

add_executable(aicpu_scheduler ${SRC_LIST})
#SET_TARGET_PROPERTIES(aicpu_scheduler PROPERTIES LINK_FLAGS "-nostartfiles")

add_custom_command(TARGET aicpu_scheduler
    POST_BUILD
    COMMAND ${CMAKE_OBJDUMP} -d aicpu_scheduler > aicpu_scheduler.dump.s
)

add_executable(header2json ${CMAKE_CURRENT_SOURCE_DIR}/tools/header2json.c)