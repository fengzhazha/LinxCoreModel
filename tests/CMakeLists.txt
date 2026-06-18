cmake_minimum_required(VERSION 3.12)

# 让 CMake 通过环境变量提示 gtest 的安装前缀
if(DEFINED ENV{GTEST_ROOT})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{GTEST_ROOT}")
endif()

# 优先走 Config 包
find_package(GTest CONFIG QUIET)

# 如果没有 Config 包，则尝试模块模式
if(NOT GTest_FOUND)
  find_package(GTest REQUIRED)
endif()

# 不再使用 gtest_discover_tests
# 只负责组织子目录构建
add_subdirectory(minst_test)
