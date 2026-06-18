`BlockISA`编译测试主要包含`Toolchain`编译生成可执行文件(llvm编译器和binutils（包含汇编器和链接器）)，`Function Model`运行可执行文件两个步骤。

整个流程涉及两个代码仓：

- `BlockISA`仓（binutils和Function Model，sim-refact分支）：ssh://contributor:2222/Graphflow/BlockISA.git（用户手动clone并使用build.sh进行构建编译器和binutils）
- `linx-llvm`仓（llvm编译器，dev-gfu分支）：ssh://contributor:2222/linx/linx-llvm.git（dev-gfu分支），会通过build.sh自动clone代码仓并进行构建



## 一、构建Toolchain
### 1. 新建工作目录
```
  mkdir <WORK_PATH>
```
### 2. 获取`BlockISA`代码仓
```
  cd <WORK_PATH>
  git clone -b sim-refact ssh://contributor:2222/Graphflow/BlockISA.git
```

### 3. 构建
#### （1）更新代码并全部重新构建
```
  cd <WORK_PATH>/BlockISA
  git checkout sim-refact
  cd build
  ./build.sh -v llvm12.0 -u 2 -d 2 -n 128
```
#### （2）不更新代码，工具选择性构建
```
  cd <WORK_PATH>/BlockISA
  git checkout sim-refact
  cd build
```

只编译llvm:
```
./build.sh -v llvm12.0 -u 0 -d 2 -n 128 -b llvm
```
只编译binutils:
```
./build.sh -v llvm12.0 -u 0 -d 2 -n 128 -b binutils
```
编译llvm和binutils:
```
./build.sh -v llvm12.0 -u 0 -d 2 -n 128 -b llvm-binutils
```
--------------------

## 二、使用Toolchain生成可执行文件
```
<WORK_PATH>/BlockISA/build/linx_blockisa_llvm/bin/clang --target=riscv64 -march=rv64im -mabi=lp64 -O3 test.c -o test.bin
```
--------------------
## 三、使用Function Model运行可执行文件
参考说明：https://example.com/Graphflow/BlockISA/home
### 1. 构建Function Model
```
  cd <WORK_PATH>/BlockISA
  git checkout sim-refact
  rm -rf build
  mkdir build
  cd build
  cmake ..
  make -j
```
### 2. 使用Function Model
specified start PC and end PC
```
cd BlockISA
./bin/gfrun -r 54 -f test_loop.o -X 2
```
default start PC and end PC (entry point address and default specified PC)
```
cd BlockISA
./bin/gfrun -f test_loop.o
```
Symbol explanation:

-r: to indicate the stop PC of the elf (hex)
-f: to indicate the file name of elf
-X: to indicate the start PC of the elf (hex)
-c: to indicate the maximum number of blocks in the elf to be run (dec)
-?: to print help info
-t: to enable the trace mode (1: enable 0: disable)
-e: to enable the trace mode by indicating the begin of trace PC (BPC only)

## 四、通过csv可视化块并行度

### 生成可视化csv，cycle的生成只考虑了get/set依赖、load/store依赖，无依赖cycle从1开始

- load with 4 cycle

```
./gfrun -m c -f xx/xx/slice_name.json -t 1 -a 3 | python3 xx/BlockISA/scripts/cycle_csv.py -i xx/xx/slice_name.json
```

- load with 1 cycle

```
./gfrun -m c -f xx/xx/slice_name.json -t 1 | python3 xx/BlockISA/scripts/cycle_csv.py -i xx/xx/slice_name.json
```

* -m: to indicate block number in csv, it must be less than 5000
* -a: to indicate load with 4 cycle if set to 4. Default a is 0 with 1 cycle load 
* -f: to indicate json path
* -i: to indicate json path(same with -f)

## 五、统计含Store指令的块占比

### 生成log

```
./gfrun -m c -f xx/xx/slice_name.json -a 1 > xx/xx/slice_name_store_num.log
```

* -a: to indicate enable block with store count if set to 1. Default value is 0
* -f: to indicate json path

### 画直方图

```
python draw_sum_histogram.py -f path/to/all_logs -m 5
```

* -f: to indicate path including all logs 


## 六、统计共享寄存器值

### 生成csv

```
./gfrun -m c -f xx/xx/slice_name.json -t 1 -a 2 | python3 xx/BlockISA/scripts/ur_value_statistic.py -i xx/xx/slice_name.json
```

* -a: to indicate enable ur value statistic if set to 2. Default value is 0
* -f: to indicate json path
* -i: to indicate json path(same with -f)

### 画直方图

```
python draw_sum_histogram.py -f path/to/all_csvs -m 0
```

* -f: to indicate path including all csvs

## 七、统计共享寄存器在连续两次写入期间数据

### 生成csv

```
./gfrun -m c -f xx/xx/slice_name.json -t 1 | python3 xx/BlockISA/scripts/ur_depend_statistic.py -i xx/xx/slice_name.json
```

* -f: to indicate json path
* -i: to indicate json path(same with -f)

### 画直方图

#### 统计共享寄存器在连续两次写入期间读取的块数

```
python draw_sum_histogram.py -f path/to/all_csvs -m 1
```

* -f: to indicate path including all csvs

#### 统计共享寄存器在连续两次写入期间每次读取的块id减写入块id

```
python draw_sum_histogram.py -f path/to/all_csvs -m 4
```

* -f: to indicate path including all csvs

## 八、查看PE使用率

### Block进入PE ROB作为起点，从PE ROB离开作为终点，生成trace file

```
./gfsim -w xx -m xx -f xx/xx/slice_name.json -s core.bp_mode=0 core.soc_enable=true -t 5 | python ~/tmp/BlockISA/scripts/block_load_json.py -i xx/xx/slice_name.json
```

* -f: to indicate json path
* -i: to indicate json path(same with -f)

### 使用Perfetto可视化

- 打开https://ui.perfetto.dev/
- 点击左侧Open trace file导入trace file即可查看PE使用率。由于源码中有Process和Thread，该处Process和Thread和常规概念不同——使用不同Process代表不同PE，不同Block在PE rob的生命周期会有重叠，使用不同Thread避免重叠。