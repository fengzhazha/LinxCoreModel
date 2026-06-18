cmake_minimum_required(VERSION 3.8)

include_directories(${PROJECT_SOURCE_DIR}/include)
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/data)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
file(GLOB_RECURSE ASM_SOURCES ${PROJECT_SOURCE_DIR}/testcases/*.s)

add_executable(graph_bfs ${SRC_LIST})
#SET_TARGET_PROPERTIES(graph_bfs PROPERTIES LINK_FLAGS "-nostartfiles")

add_custom_command(TARGET graph_bfs
    POST_BUILD
    COMMAND ${CMAKE_OBJDUMP} -d graph_bfs > graph_bfs.dump.s
)