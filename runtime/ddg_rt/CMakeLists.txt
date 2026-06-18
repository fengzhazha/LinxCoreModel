cmake_minimum_required(VERSION 3.17)
project(ddg-rt-test LANGUAGES CXX)

option(LINX "Compile for LinxISA" [OFF])

if(LINX)
set(CMAKE_CXX_COMPILER "/home/zhangzhendong/workspaces/linx-toolchain-v0.40/output/linx_blockisa_llvm_musl/bin/clang++")
set(BST_DRIVER "/home/zhangzhendong/workspaces/bst-driver/output")
#set(CMAKE_CXX_COMPILER ${BST_DRIVER}/bin/bstcc)
set(CMAKE_CXX_FLAGS "--target=linx64v4-linux-musl -O2 -mllvm -target-for-light-core=false -mllvm -enable-csel=true -fno-jump-tables -DLINXV4")
set(CMAKE_CXX_COMPILE_OBJECT "<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> -c <SOURCE> <FLAGS> -o <OBJECT> ")
endif()

# Add include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${BST_DRIVER}/include)

# Add sources
file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

# Add executable
add_executable(ddg-rt-test ${SOURCES})

TARGET_LINK_LIBRARIES(ddg-rt-test pthread)
