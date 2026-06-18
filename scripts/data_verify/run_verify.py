#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import sys
import re
import shutil
import argparse
import subprocess
from pathlib import Path

try:
    import numpy as np
except ImportError:
    print("ERROR: numpy is required")
    sys.exit(1)


def norm_status(s):
    return "" if s is None else str(s).strip().lower()


def rm_rf(path: Path):
    if path.exists():
        if path.is_dir():
            shutil.rmtree(path)
        else:
            path.unlink()


def run_cmd(cmd, cwd=None):
    proc = subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def resolve_janus_root(janus_path_str):
    janus_root = Path(janus_path_str).expanduser().resolve()
    if not janus_root.exists():
        raise FileNotFoundError(f"JanusCoreBench path not found: {janus_root}")
    return janus_root


def find_run_result_check(janus_root: Path):
    script = janus_root / "test" / "scripts" / "run_result_check.py"
    if not script.exists():
        raise FileNotFoundError(f"run_result_check.py not found: {script}")
    return script


def get_elf_name_no_ext(elf_path: Path):
    return elf_path.stem


def parse_run_result_check_output(output: str, elf_name: str):
    run_status = None
    chk_status = None
    loss = None
    target_line = None

    lines = output.splitlines()

    for line in lines:
        if elf_name in line and "run_status" in line and "chk_status" in line:
            target_line = line.strip()

    if target_line is None:
        for line in reversed(lines):
            if "run_status" in line and "chk_status" in line:
                target_line = line.strip()
                break

    if target_line:
        m = re.search(
            r"run_status:\s*(.*?)\s+chk_status:\s*(.*?)\s+loss:\s*([^\s]+)",
            target_line,
            re.IGNORECASE,
        )
        if m:
            run_status = m.group(1).strip()
            chk_status = m.group(2).strip()
            loss = m.group(3).strip()

    return run_status, chk_status, loss, target_line


def call_run_result_check(janus_root: Path, elf_path: Path):
    cmd = ["python3", "./test/scripts/run_result_check.py", "-d", str(elf_path)]
    code, out, err = run_cmd(cmd, cwd=janus_root)

    merged = (out or "") + ("\n" + err if err else "")
    elf_name = elf_path.stem
    run_status, chk_status, loss, line = parse_run_result_check_output(merged, elf_name)

    return {
        "returncode": code,
        "stdout": out,
        "stderr": err,
        "merged": merged,
        "run_status": run_status,
        "chk_status": chk_status,
        "loss": loss,
        "matched_line": line,
    }


def get_compare_dir(janus_root: Path, elf_name: str):
    compare_dir = janus_root / "compare" / elf_name
    if not compare_dir.exists() or not compare_dir.is_dir():
        raise FileNotFoundError(f"Generated compare dir not found: {compare_dir}")
    return compare_dir


def check_and_prepare_res_files(compare_dir: Path):
    old_res = compare_dir / "res.bin"
    qemu_res = compare_dir / "res_qemu.bin"

    print(f"rename前检查: {old_res} exists ? {old_res.exists()}")

    if qemu_res.exists():
        qemu_res.unlink()

    qemu_generated = old_res.exists()

    if qemu_generated:
        old_res.rename(qemu_res)
        print(f"已将 qemu 输出文件改名: {old_res.name} -> {qemu_res.name}")
    else:
        print("qemu 未能产生输出文件")

    old_res.touch(exist_ok=True)

    return (qemu_res if qemu_generated else None), old_res, qemu_generated


def run_gfrun(gfrun_path: Path, elf_path: Path, work_dir: Path):
    cmd = [str(gfrun_path), "-f", str(elf_path)]
    returncode, stdout, stderr = run_cmd(cmd, cwd=work_dir)
    return {
        "returncode": returncode,
        "stdout": stdout,
        "stderr": stderr,
    }


def normalize_dtype(dtype_str: str):
    if not dtype_str:
        return "fp32"

    s = dtype_str.strip().lower()
    s = s.replace(" ", "")
    s = s.replace("-", "_")

    alias = {
        "fp64": "fp64",
        "float64": "fp64",
        "double": "fp64",
        "fp32": "fp32",
        "float32": "fp32",
        "float": "fp32",
        "fp16": "fp16",
        "float16": "fp16",
        "half": "fp16",
        "fp8": "fp8",
        "fp8(e4m3)": "fp8",
        "fp8_e4m3": "fp8",
        "e4m3": "fp8",
        "fp8_1": "fp8_1",
        "fp8_1(e5m2)": "fp8_1",
        "fp8(e5m2)": "fp8_1",
        "fp8_e5m2": "fp8_1",
        "e5m2": "fp8_1",
        "fp6": "fp6",
        "fp6(e3m2)": "fp6",
        "fp6_e3m2": "fp6",
        "e3m2": "fp6",
        "fp6_1": "fp6_1",
        "fp6_1(e2m3)": "fp6_1",
        "fp6(e2m3)": "fp6_1",
        "fp6_e2m3": "fp6_1",
        "e2m3": "fp6_1",
        "fp4": "fp4",
        "fp4(e2m1)": "fp4",
        "fp4_e2m1": "fp4",
        "fp4_1": "fp4_1",
        "fp4_1(e1m2)": "fp4_1",
        "fp4(e1m2)": "fp4_1",
        "fp4_e1m2": "fp4_1",
        "e1m2": "fp4_1",
        "int64": "int64",
        "int32": "int32",
        "int16": "int16",
        "int8": "int8",
        "int4": "int4",
        "uint64": "uint64",
        "uint32": "uint32",
        "uint16": "uint16",
        "uint8": "uint8",
        "uint4": "uint4",
    }

    if s not in alias:
        raise ValueError(f"Unsupported dtype: {dtype_str}")
    return alias[s]


def is_float_dtype(dtype: str):
    return dtype in {
        "fp64", "fp32", "fp16",
        "fp8", "fp8_1",
        "fp6", "fp6_1",
        "fp4", "fp4_1",
    }


def is_int_dtype(dtype: str):
    return dtype in {
        "int64", "int32", "int16", "int8", "int4",
        "uint64", "uint32", "uint16", "uint8", "uint4",
    }


def ieee_custom_to_float(bits: int, total_bits: int, exp_bits: int, mant_bits: int):
    sign = (bits >> (exp_bits + mant_bits)) & 0x1
    exp_mask = (1 << exp_bits) - 1
    mant_mask = (1 << mant_bits) - 1

    exp = (bits >> mant_bits) & exp_mask
    mant = bits & mant_mask

    bias = (1 << (exp_bits - 1)) - 1 if exp_bits > 0 else 0
    sign_val = -1.0 if sign else 1.0

    if exp == 0:
        if mant == 0:
            return sign_val * 0.0
        return sign_val * (mant / (2 ** mant_bits)) * (2 ** (1 - bias))
    if exp == exp_mask:
        if mant == 0:
            return float("-inf") if sign else float("inf")
        return float("nan")
    return sign_val * (1.0 + mant / (2 ** mant_bits)) * (2 ** (exp - bias))


def unpack_int4(data: bytes, signed=True):
    vals = []
    for b in data:
        lo = b & 0xF
        hi = (b >> 4) & 0xF
        for x in (lo, hi):
            if signed and x >= 8:
                x -= 16
            vals.append(x)
    return vals


def unpack_uint4(data: bytes):
    vals = []
    for b in data:
        vals.append(b & 0xF)
        vals.append((b >> 4) & 0xF)
    return vals


def unpack_custom_float(data: bytes, total_bits: int, exp_bits: int, mant_bits: int):
    vals = []
    bitstream = []

    for byte in data:
        for i in range(8):
            bitstream.append((byte >> i) & 1)

    n = len(bitstream) // total_bits
    for i in range(n):
        bits = 0
        for j in range(total_bits):
            bits |= (bitstream[i * total_bits + j] << j)
        vals.append(ieee_custom_to_float(bits, total_bits, exp_bits, mant_bits))

    return vals


def read_bin_by_dtype(bin_path: Path, dtype: str):
    data = bin_path.read_bytes()
    dtype = normalize_dtype(dtype)

    if dtype == "fp64":
        return np.frombuffer(data, dtype=np.float64)
    if dtype == "fp32":
        return np.frombuffer(data, dtype=np.float32)
    if dtype == "fp16":
        return np.frombuffer(data, dtype=np.float16)
    if dtype == "int64":
        return np.frombuffer(data, dtype=np.int64)
    if dtype == "int32":
        return np.frombuffer(data, dtype=np.int32)
    if dtype == "int16":
        return np.frombuffer(data, dtype=np.int16)
    if dtype == "int8":
        return np.frombuffer(data, dtype=np.int8)
    if dtype == "uint64":
        return np.frombuffer(data, dtype=np.uint64)
    if dtype == "uint32":
        return np.frombuffer(data, dtype=np.uint32)
    if dtype == "uint16":
        return np.frombuffer(data, dtype=np.uint16)
    if dtype == "uint8":
        return np.frombuffer(data, dtype=np.uint8)
    if dtype == "int4":
        return np.array(unpack_int4(data, signed=True), dtype=np.int32)
    if dtype == "uint4":
        return np.array(unpack_uint4(data), dtype=np.uint32)
    if dtype == "fp8":
        return np.array(unpack_custom_float(data, 8, 4, 3), dtype=np.float64)
    if dtype == "fp8_1":
        return np.array(unpack_custom_float(data, 8, 5, 2), dtype=np.float64)
    if dtype == "fp6":
        return np.array(unpack_custom_float(data, 6, 3, 2), dtype=np.float64)
    if dtype == "fp6_1":
        return np.array(unpack_custom_float(data, 6, 2, 3), dtype=np.float64)
    if dtype == "fp4":
        return np.array(unpack_custom_float(data, 4, 2, 1), dtype=np.float64)
    if dtype == "fp4_1":
        return np.array(unpack_custom_float(data, 4, 1, 2), dtype=np.float64)

    raise ValueError(f"Unsupported dtype: {dtype}")


def bin_to_txt(bin_path: Path, dtype: str):
    values = read_bin_by_dtype(bin_path, dtype)
    txt_path = bin_path.with_suffix(".txt")
    with open(txt_path, "w", encoding="utf-8") as f:
        for idx, v in enumerate(values.tolist(), start=1):
            f.write(f"{idx}: {v}\n")
    return txt_path


def convert_all_bins_to_txt(dir_path: Path, dtype: str):
    txt_files = []
    for p in sorted(dir_path.iterdir()):
        if p.is_file() and p.suffix == ".bin":
            txt_files.append(bin_to_txt(p, dtype))
    return txt_files


def get_tolerance(dtype: str):
    dtype = normalize_dtype(dtype)

    if dtype == "fp64":
        return 1e-12, 1e-12
    if dtype == "fp32":
        return 1e-5, 1e-6
    if dtype == "fp16":
        return 1e-3, 1e-3
    if dtype in {"fp8", "fp8_1"}:
        return 1e-1, 1e-1
    if dtype in {"fp6", "fp6_1"}:
        return 2e-1, 2e-1
    if dtype in {"fp4", "fp4_1"}:
        return 5e-1, 5e-1
    return 0.0, 0.0


def compare_integer_arrays(lhs, rhs):
    if lhs.shape != rhs.shape:
        return False, f"shape mismatch: lhs={lhs.shape}, rhs={rhs.shape}", None

    same = np.array_equal(lhs, rhs)
    if same:
        return True, "exact_equal", None

    mismatch_idx = np.flatnonzero(lhs != rhs)
    idx = int(mismatch_idx[0])
    detail = {
        "index": idx,
        "lhs": lhs[idx].item(),
        "rhs": rhs[idx].item(),
        "abs_err": abs(int(lhs[idx]) - int(rhs[idx])),
        "rel_err": None,
    }
    return False, "not_equal", detail


def compare_float_arrays(lhs, rhs, dtype: str):
    if lhs.shape != rhs.shape:
        return False, f"shape mismatch: lhs={lhs.shape}, rhs={rhs.shape}", None

    rtol, atol = get_tolerance(dtype)
    close_mask = np.isclose(lhs, rhs, rtol=rtol, atol=atol, equal_nan=True)
    same = bool(np.all(close_mask))

    if same:
        return True, f"allclose(rtol={rtol}, atol={atol})", None

    mismatch_idx = np.flatnonzero(~close_mask)
    idx = int(mismatch_idx[0])

    lhs_v = float(lhs[idx])
    rhs_v = float(rhs[idx])
    abs_err = abs(lhs_v - rhs_v)

    if np.isnan(lhs_v) and np.isnan(rhs_v):
        rel_err = 0.0
    elif rhs_v == 0:
        rel_err = 0.0 if abs_err == 0 else float("inf")
    else:
        rel_err = abs_err / abs(rhs_v)

    detail = {
        "index": idx,
        "lhs": lhs_v,
        "rhs": rhs_v,
        "abs_err": abs_err,
        "rel_err": rel_err,
    }
    return False, f"allclose(rtol={rtol}, atol={atol})", detail


def compare_two_bins(path_a: Path, path_b: Path, dtype: str):
    if not path_a.exists():
        return False, f"{path_a.name} not found", None
    if not path_b.exists():
        return False, f"{path_b.name} not found", None

    dtype = normalize_dtype(dtype)
    arr_a = read_bin_by_dtype(path_a, dtype)
    arr_b = read_bin_by_dtype(path_b, dtype)

    if is_int_dtype(dtype):
        return compare_integer_arrays(arr_a, arr_b)
    if is_float_dtype(dtype):
        return compare_float_arrays(arr_a.astype(np.float64), arr_b.astype(np.float64), dtype)

    return False, "unsupported dtype", None


def print_compare_result(title: str, path_a: Path, path_b: Path, dtype: str):
    print(f"==== {title} ====")
    print(f"lhs: {path_a}")
    print(f"rhs: {path_b}")

    same, compare_detail, mismatch = compare_two_bins(path_a, path_b, dtype)
    print(f"compare detail: {compare_detail}")
    print(f"same ? {same}")

    if mismatch is not None:
        print("first mismatch detail:")
        print(f"  index   : {mismatch['index']}")
        print(f"  lhs     : {mismatch['lhs']}")
        print(f"  rhs     : {mismatch['rhs']}")
        print(f"  abs_err : {mismatch['abs_err']}")
        print(f"  rel_err : {mismatch['rel_err']}")

    return same, compare_detail, mismatch


def pass_fail_str(same: bool):
    return "PASS" if same else "FAIL"


def final_message(run_status, chk_status, same_gfrun_golden):
    rs = norm_status(run_status)
    cs = norm_status(chk_status)

    if rs == "fail":
        if same_gfrun_golden:
            return "qemu未跑过，gfrun结果校验通过"
        "qemu未跑过，gfrun结果校验未通过"

    if rs == "pass" and cs == "fail":
        if same_gfrun_golden:
            return "qemu结果校验未通过，gfrun结果校验通过"
        return "qemu结果校验未通过，gfrun结果校验未通过"

    if rs == "pass" and cs == "pass":
        if same_gfrun_golden:
            return "qemu结果校验通过，gfrun结果校验通过"
        return "qemu结果校验通过，gfrun结果校验未通过"

    return f"无法判定：run_status={run_status}, chk_status={chk_status}, gfrun_same={same_gfrun_golden}"


def should_cleanup_case6(run_status, chk_status, same_gfrun_golden):
    rs = norm_status(run_status)
    cs = norm_status(chk_status)
    return rs == "pass" and cs == "pass" and same_gfrun_golden


def cleanup_compare_dir(janus_root: Path, elf_name: str):
    compare_dir = janus_root / "compare" / elf_name
    rm_rf(compare_dir)
    return compare_dir


def main():
    parser = argparse.ArgumentParser(
        description="Run run_result_check.py + rename qemu res + gfrun + bin2txt + compare"
    )
    parser.add_argument("-p", "--program", required=True, help="gfrun程序路径")
    parser.add_argument("-f", "--file", required=True, help="elf文件路径")
    parser.add_argument("-d", "--dir", required=True, help="JanusCoreBench库路径")
    parser.add_argument("--dtype", default="FP32", help="数据格式，默认 FP32")
    args = parser.parse_args()

    gfrun_path = Path(args.program).expanduser().resolve()
    elf_path = Path(args.file).expanduser().resolve()
    janus_root = resolve_janus_root(args.dir)
    find_run_result_check(janus_root)

    if not gfrun_path.exists():
        raise FileNotFoundError(f"gfrun not found: {gfrun_path}")
    if not elf_path.exists():
        raise FileNotFoundError(f"elf not found: {elf_path}")

    dtype = normalize_dtype(args.dtype)
    elf_name = get_elf_name_no_ext(elf_path)

    print("==== Step1: 在 JanusCoreBench 根目录调用 run_result_check.py ====")
    rr = call_run_result_check(janus_root, elf_path)
    print(rr["merged"])
    print(
        f"解析结果: run_status={rr['run_status']}, "
        f"chk_status={rr['chk_status']}, loss={rr['loss']}"
    )

    print("==== Step2: 检查并处理 qemu 的 res.bin ====")
    compare_dir = get_compare_dir(janus_root, elf_name)
    print(f"compare dir: {compare_dir}")
    qemu_res_bin, gfrun_res_bin, qemu_generated = check_and_prepare_res_files(compare_dir)
    print(f"qemu result path : {qemu_res_bin if qemu_res_bin else 'None'}")
    print(f"gfrun result path: {gfrun_res_bin}")

    print("==== Step3: 执行 gfrun ====")
    gfr = run_gfrun(gfrun_path, elf_path, compare_dir)
    print("gfrun stdout:")
    print(gfr["stdout"])
    print("gfrun stderr:")
    print(gfr["stderr"])
    print(f"gfrun returncode: {gfr['returncode']}")

    print("==== Step4: bin 转 txt ====")
    txt_files = convert_all_bins_to_txt(compare_dir, dtype)
    for tf in txt_files:
        print(f"generated: {tf}")

    golden_bin = compare_dir / "golden.bin"

    if qemu_generated and qemu_res_bin is not None:
        same_qemu_golden, _, _ = print_compare_result(
            "Step5-1: 比较 res_qemu.bin 与 golden.bin",
            qemu_res_bin,
            golden_bin,
            dtype,
        )
    else:
        same_qemu_golden = False
        print("==== Step5-1: 比较 res_qemu.bin 与 golden.bin ====")
        print("跳过：qemu 未产生 res_qemu.bin")

    same_gfrun_golden, _, _ = print_compare_result(
        "Step5-2: 比较 res.bin 与 golden.bin",
        gfrun_res_bin,
        golden_bin,
        dtype,
    )

    if qemu_generated and qemu_res_bin is not None:
        same_gfrun_qemu, _, _ = print_compare_result(
            "Step5-3: 比较 res.bin 与 res_qemu.bin",
            gfrun_res_bin,
            qemu_res_bin,
            dtype,
        )
    else:
        same_gfrun_qemu = False
        print("==== Step5-3: 比较 res.bin 与 res_qemu.bin ====")
        print("跳过：qemu 未产生 res_qemu.bin")

    print("==== Summary ====")
    print(f"qemu_vs_golden  : {pass_fail_str(same_qemu_golden) if qemu_generated else 'SKIP'}")
    print(f"gfrun_vs_golden : {pass_fail_str(same_gfrun_golden)}")
    print(f"gfrun_vs_qemu   : {pass_fail_str(same_gfrun_qemu) if qemu_generated else 'SKIP'}")

    print("==== Final Result ====")
    result_msg = final_message(rr["run_status"], rr["chk_status"], same_gfrun_golden)
    print(result_msg)

    if should_cleanup_case6(rr["run_status"], rr["chk_status"], same_gfrun_golden):
        print("==== Cleanup: 命中情况(6)，删除 JanusCoreBench/compare 下该测例目录 ====")
        deleted_dir = cleanup_compare_dir(janus_root, elf_name)
        print(f"deleted: {deleted_dir}")
    else:
        print("==== Cleanup: 保留目录及文件 ====")
        print("原因: 未命中情况(6)，保留 compare 目录便于排查")


if __name__ == "__main__":
    main()
