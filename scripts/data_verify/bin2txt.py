#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import math
from pathlib import Path

# 调用：python3 bin2txt.py //user/test/data.bin fp16 
# 注：生成的同名txt可读文件会放到bin所在目录下

FORMAT_TABLE = {
    "fp64":   {"kind": "float", "bits": 64, "e": 11, "m": 52},
    "fp32":   {"kind": "float", "bits": 32, "e": 8,  "m": 23},
    "fp16":   {"kind": "float", "bits": 16, "e": 5,  "m": 10},

    "fp8":    {"kind": "float", "bits": 8,  "e": 4,  "m": 3},  # e4m3
    "fp8_1":  {"kind": "float", "bits": 8,  "e": 5,  "m": 2},  # e5m2

    "fp6":    {"kind": "float", "bits": 6,  "e": 3,  "m": 2},  # e3m2
    "fp6_1":  {"kind": "float", "bits": 6,  "e": 2,  "m": 3},  # e2m3

    "fp4":    {"kind": "float", "bits": 4,  "e": 2,  "m": 1},  # e2m1
    "fp4_1":  {"kind": "float", "bits": 4,  "e": 1,  "m": 2},  # e1m2

    "int64":  {"kind": "int", "bits": 64, "signed": True},
    "int32":  {"kind": "int", "bits": 32, "signed": True},
    "int16":  {"kind": "int", "bits": 16, "signed": True},
    "int8":   {"kind": "int", "bits": 8,  "signed": True},
    "int4":   {"kind": "int", "bits": 4,  "signed": True},

    "uint64": {"kind": "int", "bits": 64, "signed": False},
    "uint32": {"kind": "int", "bits": 32, "signed": False},
    "uint16": {"kind": "int", "bits": 16, "signed": False},
    "uint8":  {"kind": "int", "bits": 8,  "signed": False},
    "uint4":  {"kind": "int", "bits": 4,  "signed": False},
}


def read_packed_values_lsb(data: bytes, bits_per_value: int):
    """
    按 little-endian bitstream 读取定长 bit 字段：
    - byte 内 bit0 -> bit7 顺序
    - 值之间连续拼接
    返回 (values, leftover_bits)
    """
    total_bits = len(data) * 8
    n = total_bits // bits_per_value
    leftover = total_bits % bits_per_value

    out = []
    bit_pos = 0
    mask = (1 << bits_per_value) - 1

    for _ in range(n):
        v = 0
        # 取 bits_per_value个bit，最低位先拼
        for i in range(bits_per_value):
            byte_idx = (bit_pos + i) // 8
            bit_idx = (bit_pos + i) % 8
            bit = (data[byte_idx] >> bit_idx) & 1
            v |= (bit << i)
        out.append(v & mask)
        bit_pos += bits_per_value

    return out, leftover


def decode_ieee_like_float(raw: int, e_bits: int, m_bits: int) -> float:
    sign_bit = (raw >> (e_bits + m_bits)) & 0x1
    exp_mask = (1 << e_bits) - 1
    man_mask = (1 << m_bits) - 1

    exponent = (raw >> m_bits) & exp_mask
    mantissa = raw & man_mask

    sign = -1.0 if sign_bit else 1.0
    bias = (1 << (e_bits - 1)) - 1 if e_bits > 0 else 0
    exp_all_ones = exp_mask

    if e_bits == 0:
        frac = mantissa / (1 << m_bits) if m_bits > 0 else 0.0
        return sign * frac

    if exponent == 0:
        if mantissa == 0:
            return math.copysign(0.0, -1.0 if sign_bit else 1.0)
        frac = mantissa / (1 << m_bits)
        return sign * (2.0 ** (1 - bias)) * frac

    if exponent == exp_all_ones:
        if mantissa == 0:
            return -math.inf if sign_bit else math.inf
        return math.nan

    frac = 1.0 + mantissa / (1 << m_bits)
    return sign * (2.0 ** (exponent - bias)) * frac


def decode_twos_complement(raw: int, bits: int) -> int:
    sign_bit = 1 << (bits - 1)
    if raw & sign_bit:
        return raw - (1 << bits)
    return raw


def format_value(v):
    # 对 nan/inf 友好输出
    if isinstance(v, float):
        if math.isnan(v):
            return "nan"
        if math.isinf(v):
            return "-inf" if v < 0 else "inf"
        # 用 repr 保留较好精度
        return repr(v)
    return str(v)


def convert_file(input_path: Path, fmt: str):
    spec = FORMAT_TABLE[fmt]
    data = input_path.read_bytes()

    raw_vals, leftover = read_packed_values_lsb(data, spec["bits"])

    if leftover != 0:
        print(f"[WARN] 文件末尾有 {leftover} 个bit不足一个完整元素，已忽略。")

    decoded = []
    if spec["kind"] == "float":
        e_bits = spec["e"]
        m_bits = spec["m"]
        for rv in raw_vals:
            decoded.append(decode_ieee_like_float(rv, e_bits, m_bits))
    else:
        signed = spec["signed"]
        bits = spec["bits"]
        if signed:
            decoded = [decode_twos_complement(rv, bits) for rv in raw_vals]
        else:
            decoded = raw_vals

    output_path = input_path.with_suffix(".txt")
    with output_path.open("w", encoding="utf-8") as f:
        for v in decoded:
            f.write(format_value(v) + "\n")

    print(f"[OK] 转换完成：{output_path}")
    print(f"[INFO] 元素个数：{len(decoded)}")


def main():
    parser = argparse.ArgumentParser(
        description="把指定格式的 .bin 二进制文件转换为同名 .txt"
    )
    parser.add_argument("bin_file", help="输入 .bin 文件路径")
    parser.add_argument(
        "fmt",
        choices=list(FORMAT_TABLE.keys()),
        help="数据格式: " + ", ".join(FORMAT_TABLE.keys())
    )

    args = parser.parse_args()

    input_path = Path(args.bin_file).expanduser().resolve()
    if not input_path.exists():
        raise FileNotFoundError(f"文件不存在: {input_path}")
    if not input_path.is_file():
        raise ValueError(f"不是文件: {input_path}")
    if input_path.suffix.lower() != ".bin":
        print(f"[WARN] 输入文件后缀不是 .bin: {input_path.name}")

    convert_file(input_path, args.fmt)


if __name__ == "__main__":
    main()
