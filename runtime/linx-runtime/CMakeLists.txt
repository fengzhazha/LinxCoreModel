cmake_minimum_required(VERSION 3.8)

project(linda-rt LANGUAGES C ASM)

option(VALIDATE "Compile for General Architecture" [OFF])

if(NOT VALIDATE)
set(CMAKE_C_COMPILER "/home/zhangzhendong/workspaces/linx-toolchain-v0.40/output/linx_blockisa_llvm_musl/bin/clang")
set(CMAKE_ASM_COMPILER "/home/zhangzhendong/workspaces/linx-toolchain-v0.40/output/linx_blockisa_llvm_musl/bin/clang")
set(CMAKE_OBJDUMP "/home/zhangzhendong/workspaces/linx-toolchain-v0.40/output/linx_blockisa_llvm_musl/bin/llvm-objdump")
set(CMAKE_C_FLAGS "--target=linx64v4-linux-musl -O2 -mllvm -target-for-light-core=false -mllvm -enable-csel=true -fno-jump-tables -DLINXV4")
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)

add_library(linda-rt STATIC ${SRC_LIST})

add_subdirectory(testcases/graph_bfs)
add_subdirectory(testcases/aicpu_scheduler)