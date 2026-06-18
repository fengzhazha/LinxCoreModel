#!/bin/sh

ROOT_DIR=$(cd $(dirname $0) && pwd)

# 调用gfrun中的decodetree脚本,依照qemu的.decode解码文件,生成对应的C++文件，并放入指定位置
python3 $ROOT_DIR/decodetree.py $ROOT_DIR/decodefiles/block16.decode --static-decode=DecodeInst16 --binarywidth=16 -o $ROOT_DIR/generatedfiles/decode-inst16.cpp
python3 $ROOT_DIR/decodetree.py $ROOT_DIR/decodefiles/block32.decode --static-decode=DecodeInst32 --binarywidth=32 -o $ROOT_DIR/generatedfiles/decode-inst32.cpp
# python3 $ROOT_DIR/decodetree.py $ROOT_DIR/decodefiles/block32_private_aux.decode  --static-decode=DecodeBlock32PrivateAux  --binarywidth=32 -o $ROOT_DIR/generatedfiles/decode-private_aux.cpp
# python3 $ROOT_DIR/decodetree.py $ROOT_DIR/decodefiles/block32_private_fp.decode   --static-decode=DecodeBlock32PrivateFp   --binarywidth=32 -o $ROOT_DIR/generatedfiles/decode-private_fp.cpp
python3 $ROOT_DIR/decodetree.py $ROOT_DIR/decodefiles/block48.decode --static-decode=DecodeInst48 --binarywidth=48 -o $ROOT_DIR/generatedfiles/decode-inst48.cpp
python3 $ROOT_DIR/decodetree.py $ROOT_DIR/decodefiles/block64.decode --static-decode=DecodeInst64 --binarywidth=64 -o $ROOT_DIR/generatedfiles/decode-inst64.cpp

python3 $ROOT_DIR/encodetree.py $ROOT_DIR/decodefiles/block16.decode $ROOT_DIR/generatedfiles/decode-inst16.cpp $ROOT_DIR/generatedfiles/encode-inst16.cpp 16
python3 $ROOT_DIR/encodetree.py $ROOT_DIR/decodefiles/block32.decode $ROOT_DIR/generatedfiles/decode-inst32.cpp $ROOT_DIR/generatedfiles/encode-inst32.cpp 32
# python3 $ROOT_DIR/encodetree.py $ROOT_DIR/decodefiles/block48.decode $ROOT_DIR/generatedfiles/decode-block48.cpp $ROOT_DIR/generatedfiles/encode-block48.cpp 48
python3 $ROOT_DIR/encodetree.py $ROOT_DIR/decodefiles/block64.decode $ROOT_DIR/generatedfiles/decode-inst64.cpp $ROOT_DIR/generatedfiles/encode-inst64.cpp 64


