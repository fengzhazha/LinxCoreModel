本目录测试依赖 googletest。服务器无法联网时，请在可联网环境下载源码包，
拷贝到服务器后本地编译安装。

## 1. 获取源码（在可联网机器）
下载 googletest release（示例：1.14.0）：
- https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz

把 tar.gz 拷贝到服务器，例如放到：/opt/src/

## 2. 在服务器上编译安装
选择一个安装前缀（示例：/opt/gtest）：

```bash
cd /opt/src
tar xf v1.14.0.tar.gz
cd googletest-1.14.0

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/gtest

cmake --build build -j
cmake --install build
```
## 3. 设置环境变量
```bash
export CMAKE_PREFIX_PATH=/opt/gtest:$CMAKE_PREFIX_PATH
或
export GTEST_ROOT=/opt/gtest
```
## 4. 构建gtest
```bash
# cmake 增加选项-DBUILD_TESTS=ON
cmake .. -DBUILD_TESTS=ON
# 其余流程不变
```
