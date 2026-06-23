LinxISA is a block-structured Instruction Set Architecture (ISA) designed for complex computational tasks. This repository includes the BlockISA specification documents, toolchains, emulator, token model, and cycle-accurate model for research and development purposes.

## ISA Definition

The LinxISA employs a hierarchical block-structured design that encapsulates multiple related instructions into unified execution units called "blocks". This approach enhances instruction expressiveness and enables efficient block-level scheduling and optimization.

A block instruction consists of two components: a **block header** and a **block body**. The header defines branch/control and scheduling parameters, while the body contains the actual computational or memory operations. These components can be organized in different structural forms to accommodate various execution requirements.

The current BlockISA specification is version v0.55 (Updated 2025.11)

For detailed ISA documentation and manuals, please refer to the *doc* folder in this repository.

Currently, the BlockISA supports the following block types:

| Block Symbol | Block Type Name | Description |
|--|--|--|
| STD | Integer Scalar Block | General-purpose integer scalar computation |
| SYS | System Block | System control and state management operations |
| FP | Floating-point Scalar Block | Scalar floating-point computation |
| MPAR | Memory Aaccelss Parallel Block | Vectorized memory operations with parallel group execution |
| MSEQ | Memory Aaccelss Serial Block | Vectorized memory operations with serial group execution |
| VPAR | Vector Parallel Block | Vector computation with parallel group execution |
| VSEQ | Vector Serial Block | Vector computation with serial group execution |
| TMA | Tile Memory Aaccelss Block | Data movement between memory and tile registers |
| CUBE | Cube Block | Matrix computation with tiled execution |
| TEPL | Template Tile Block | Template-based tile computation |
| XB | Cross Block | Lightweight system call capability |

These block types share the first-level architectural state while maintaining type-specific second-level states, providing a flexible framework for diverse computational workloads.
[ISA Detail](https://example.com/mkdocs/project/1410/blockisa-doc/docs/site/docs/background/)

## LinxISA Execution Model
![image](./doc/image/Core%20Architecture.png)

### The architecture adopts a "main core + dedicated cores" hierarchical execution model, where the main core coordinates the work of multiple dedicated cores.
- Main core Block Control Core (BCC): responsible for scheduling dedicated cores and performing scalar processing (Turing complete).
- Dedicated core Cube Core: responsible for matrix operations.
- Dedicated core Vector Core: responsible for vector operations.
- Dedicated core Tile Memory Accelerator (TMA): responsible for data movement and interaction with external memory.

### This is a CPU-like architecture with the Tile Register as the data interaction hub.
The actual data interaction on the hardware is between the three dedicated cores: CUBE, VEC, and TMA.

## Block Model Installation

To install BlockISA model, the following prerequisite are required:
- **libelf** used for parsing ELF files: `apt-get install libelf-dev`
- **python3.8** used for parsing configurations and toml library
- **toml** used for parsing configurations `pip install toml`

We provide several models for BlockISA evaluation:

* **BlockISA Emulator or Function Model** : Run BlockISA binaries functionally.
* **BlockISA Token Model or Fast Model**: Evaluate performance using event-triggerred evalution. This model is fast but not accurate.
* **BlockISA Cycle Accurate Model**: Evaluate BlockISA binaries using cycle-accurate evaluation. This model is accurate but very slow.

To build the default models, use CMake:
```
mkdir build
cd build
cmake ..
cmake --build . -j12
```

To use GENERIC Soc, the **lib64 library of GCC-12** is required.
```
Firstly, We should  config Make Envirenment as following(applicable to specific servers):
export PATH=/software/public/gcc-12.2.0/bin:$PATH
export LD_LIBRARY_PATH=/software/public/gcc-12.2.0/lib64:$LD_LIBRARY_PATH
export CC=/software/public/gcc-12.2.0/bin/gcc
export CXX=/software/public/gcc-12.2.0/bin/g++
export CMAKE_C_COMPILER=/software/public/gcc-12.2.0/bin/gcc
export CMAKE_CXX_COMPILER=/software/public/gcc-12.2.0/bin/g++
```
Then Building and Running with GENERIC Soc old V1: (python build.py -c -D)
```
mkdir build
cd build
cmake ..  -DGENERIC_SOC=ON
make -j
```

Then Building and Running with GENERIC Soc New V2: (python build.py -c -E)
```
mkdir build
cd build
cmake ..  -GENERIC_SOC_NEW=ON
make -j
```

### Other Compilation Options
Optimization level(O0 O1 O2 O3, Default:O2)
```
cmake .. -DOPT_LEVEL=O0
```

Disable Debugging symbols
```
cmake .. -DENABLE_DEBUG_SYMBOLS=OFF
```

Enable Memory Check
```
cmake .. -DOPT_LEVEL=O0 -DENABLE_ASAN=ON
cmake .. -DOPT_LEVEL=O0 -DENABLE_UBSAN=ON
```

Enable Code Coverage check.
```
-DOPT_LEVEL=O0 -DENABLE_COVERAGE=ON -DCOVERAGE_MODE=FULL
# Data collection script:
python3 scripts/coverage.py
```

## How to use BlockISA CA Model
```
bin/gfsim -f <bin.elf>
```

To enable SOC full system simulation, please use the following command:

```
export LD_LIBRARY_PATH={$YOUR_BLOCKISA_PROJECT}/model/generic_soc/lib
cp {$YOUR_BLOCKISA_PROJECT}/model/soc/parameters .
mkdir log
bin/gfsim -f <bin.elf> -s core.soc_enable=true
```
Three extra verbose mode are supported:
* -t 1 Normal trace mode
* -t 3 Full trace mode

The following configurations can be specified by extra `--set` or `-s`.

* core.bp_mode=1
* core.simtLane=64
* core.simtEnable=true
* bctrl.bctrl_bandwidth=8

For more options, please check the wiki in this repo.
[repo wiki](https://example.com/Graphflow/BlockISA/wiki?categoryId=8),
[document wiki](https://example.com/domains/1288/wiki/262032/WIKI202508127843844)
