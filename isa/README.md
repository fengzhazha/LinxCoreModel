**Current LinxISA Verion 0.53.5**
**Linx BlockISA Detailed Description Website:[blockisa-doc](https://example.com/mkdocs/project/1410/blockisa-doc/docs/site/docs/background/)**

This folder contains the latest BlockISA specification, including:

```shell
├── ISA.h     # ISA Directory External Interface
├── MInst.cpp
├── MInst.h
├── Block.cpp
├── Block.h
├── calculate # Specific calculation logic of different types of micro-instructions
│   ├── arithmetic
│   ├── arithmetic_fp
│   ├── bit
│   ├── blockArgs
│   ├── branch
│   ├── compare
│   ├── compare_fp
│   ├── convert
│   ├── CubeCalculate.cpp
│   ├── CubeCalculate.h
│   ├── immediate
│   ├── MInstCalculator.cpp
│   ├── MInstCalculator.h
│   ├── multi_cycle
│   ├── others
│   ├── pc
│   └── setc
├── codec   # coding and decoding scripts
│   ├── build.sh
│   ├── decodetree.py
│   ├── encodetree.py
│   ├── decodefiles
│   │   ├── block16.decode
│   │   ├── block32.decode
│   │   ├── block48.decode
│   │   └── block64.decode
│   └── generatedfiles
│       ├── decode-inst16.cpp
│       ├── decode-inst32.cpp
│       ├── decode-inst48.cpp
│       ├── decode-inst64.cpp
│       ├── encode-inst16.cpp
│       ├── encode-inst32.cpp
│       └── encode-inst64.cpp
├── ISACommon # Architecture Information Definition
│   ├── BlockAttribute.h # Block Information Definition
│   ├── BlockHint.h
│   ├── BlockType.h
│   ├── BranchType.h
│   ├── BARG.h           # Architecture Register
│   ├── GPR.h
│   ├── SystemReg.h
│   ├── DecodeUtiles.cpp # Decode Information Definition
│   ├── DecodeUtiles.h
│   ├── InstInfo.h
│   ├── DataType.h       # Inst Information Definition
│   ├── LayOut.h
│   ├── EncodeLen.h
│   ├── InstGroup.h
│   ├── OpcodeManager.cpp
│   ├── OpcodeManager.h
│   ├── OperandType.h
│   ├── TileOpManager.cpp
│   ├── TileOpManager.h
│   └── BlockVerifyInfo.h # Result Verification Tool
├── CMakeLists.txt
└── README.md
```

## How to add new opcode
If you need to add a new opcode in the existing encoding space, please modify ISA.h.
The following area needs to be specified:

* Add `Opcode` definitions and register basic attributes in `isa/ISACommon/OpcodeManager`
* Add encoding and decoding scripts in the `isa/codec` directory.
* Add specific calculation logic in the `isa/calculate` directory
