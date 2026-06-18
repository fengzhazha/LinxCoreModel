#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import sys
import re
import argparse
import subprocess
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor, as_completed


FINAL_RESULT_PATTERNS = [
    "qemu未跑过，gfrun结果校验通过",
    "qemu未跑过，gfrun结果校验不确定",
    "qemu结果校验未通过，gfrun结果校验通过",
    "qemu结果校验未通过，gfrun结果校验未通过",
    "qemu结果校验通过，gfrun结果校验通过",
    "qemu结果校验通过，gfrun结果校验未通过",
]


def resolve_path(p: str) -> Path:
    return Path(p).expanduser().resolve()


def find_run_verify_script() -> Path:
    script_path = Path(__file__).resolve().parent / "run_verify.py"
    if not script_path.exists():
        raise FileNotFoundError(f"run_verify.py not found beside this script: {script_path}")
    return script_path


def find_all_elf_files(root_dir: Path):
    elf_files = []
    for p in root_dir.rglob("*"):
        if p.is_file() and p.suffix.lower() == ".elf":
            elf_files.append(p.resolve())
    return sorted(elf_files)


def run_one(run_verify_script: str, gfrun_path: str, elf_path: str, janus_root: str, dtype: str):
    cmd = [
        sys.executable,
        run_verify_script,
        "-p", gfrun_path,
        "-f", elf_path,
        "-d", janus_root,
        "--dtype", dtype,
    ]

    proc = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    stdout = proc.stdout or ""
    stderr = proc.stderr or ""
    merged = stdout + ("\n" + stderr if stderr else "")

    final_result = parse_final_result(merged)
    compare_detail = parse_compare_detail(merged)
    mismatch_detail = parse_mismatch_detail(merged)
    gfrun_returncode = parse_gfrun_returncode(merged)
    run_status, chk_status, loss = parse_run_result_meta(merged)

    reason = build_reason(
        returncode=proc.returncode,
        gfrun_returncode=gfrun_returncode,
        final_result=final_result,
        compare_detail=compare_detail,
        mismatch_detail=mismatch_detail,
        run_status=run_status,
        chk_status=chk_status,
        loss=loss,
        stdout=stdout,
        stderr=stderr,
    )

    return {
        "elf_path": elf_path,
        "proc_returncode": proc.returncode,
        "gfrun_returncode": gfrun_returncode,
        "final_result": final_result,
        "compare_detail": compare_detail,
        "mismatch_detail": mismatch_detail,
        "run_status": run_status,
        "chk_status": chk_status,
        "loss": loss,
        "reason": reason,
        "stdout": stdout,
        "stderr": stderr,
    }


def parse_final_result(text: str):
    lines = [line.strip() for line in text.splitlines()]
    final_candidates = []

    for line in lines:
        for msg in FINAL_RESULT_PATTERNS:
            if line == msg:
                final_candidates.append(line)

    if final_candidates:
        return final_candidates[-1]

    for line in reversed(lines):
        if "无法判定：" in line:
            return line.strip()

    return "未解析到最终结果"


def parse_compare_detail(text: str):
    m = re.search(r"compare detail:\s*(.+)", text)
    if m:
        return m.group(1).strip()
    return None


def parse_gfrun_returncode(text: str):
    m = re.search(r"gfrun returncode:\s*(-?\d+)", text)
    if m:
        return int(m.group(1))
    return None


def parse_run_result_meta(text: str):
    m = re.search(
        r"解析结果:\s*run_status=(.*?),\s*chk_status=(.*?),\s*loss=(.*)",
        text
    )
    if m:
        return m.group(1).strip(), m.group(2).strip(), m.group(3).strip()
    return None, None, None


def parse_mismatch_detail(text: str):
    lines = text.splitlines()
    result = {}
    capture = False

    for line in lines:
        s = line.strip()
        if s == "first mismatch detail:":
            capture = True
            continue
        if capture:
            if not s:
                break
            if s.startswith("===="):
                break
            m = re.match(r"([A-Za-z_]+)\s*:\s*(.+)", s)
            if m:
                key = m.group(1).strip()
                value = m.group(2).strip()
                result[key] = value

    return result if result else None


def build_reason(
    returncode,
    gfrun_returncode,
    final_result,
    compare_detail,
    mismatch_detail,
    run_status,
    chk_status,
    loss,
    stdout,
    stderr,
):
    if returncode != 0:
        return (
            f"run_verify.py执行失败(proc_returncode={returncode})"
            f"; gfrun_returncode={gfrun_returncode}; "
            f"run_status={run_status}, chk_status={chk_status}, loss={loss}"
        )

    if final_result == "qemu未跑过，gfrun结果校验通过":
        return (
            f"通过；qemu未跑过，但gfrun校验通过"
            f"; compare_detail={compare_detail}; gfrun_returncode={gfrun_returncode}"
        )

    if final_result == "qemu未跑过，gfrun结果校验不确定":
        reason = (
            f"未通过/不确定；qemu未跑过，且gfrun结果无法确认"
            f"; compare_detail={compare_detail}; gfrun_returncode={gfrun_returncode}"
        )
        if mismatch_detail:
            reason += (
                f"; first_mismatch=index={mismatch_detail.get('index')}, "
                f"res={mismatch_detail.get('res')}, "
                f"golden={mismatch_detail.get('golden')}, "
                f"abs_err={mismatch_detail.get('abs_err')}, "
                f"rel_err={mismatch_detail.get('rel_err')}"
            )
        return reason

    if final_result == "qemu结果校验未通过，gfrun结果校验通过":
        return (
            f"通过；qemu校验未通过，但gfrun校验通过"
            f"; compare_detail={compare_detail}; gfrun_returncode={gfrun_returncode}"
        )

    if final_result == "qemu结果校验未通过，gfrun结果校验未通过":
        reason = (
            f"未通过；qemu校验未通过，gfrun校验也未通过"
            f"; compare_detail={compare_detail}; gfrun_returncode={gfrun_returncode}"
        )
        if mismatch_detail:
            reason += (
                f"; first_mismatch=index={mismatch_detail.get('index')}, "
                f"res={mismatch_detail.get('res')}, "
                f"golden={mismatch_detail.get('golden')}, "
                f"abs_err={mismatch_detail.get('abs_err')}, "
                f"rel_err={mismatch_detail.get('rel_err')}"
            )
        return reason

    if final_result == "qemu结果校验通过，gfrun结果校验通过":
        return (
            f"通过；qemu校验通过，gfrun校验通过"
            f"; compare_detail={compare_detail}; gfrun_returncode={gfrun_returncode}"
        )

    if final_result == "qemu结果校验通过，gfrun结果校验未通过":
        reason = (
            f"未通过；qemu校验通过，但gfrun校验未通过"
            f"; compare_detail={compare_detail}; gfrun_returncode={gfrun_returncode}"
        )
        if mismatch_detail:
            reason += (
                f"; first_mismatch=index={mismatch_detail.get('index')}, "
                f"res={mismatch_detail.get('res')}, "
                f"golden={mismatch_detail.get('golden')}, "
                f"abs_err={mismatch_detail.get('abs_err')}, "
                f"rel_err={mismatch_detail.get('rel_err')}"
            )
        return reason

    if final_result.startswith("无法判定："):
        return (
            f"{final_result}; compare_detail={compare_detail}; "
            f"gfrun_returncode={gfrun_returncode}; "
            f"run_status={run_status}, chk_status={chk_status}, loss={loss}"
        )

    stderr_preview = stderr.strip().splitlines()[-1] if stderr.strip() else ""
    stdout_preview = stdout.strip().splitlines()[-1] if stdout.strip() else ""

    return (
        f"未解析到明确结论; final_result={final_result}; "
        f"compare_detail={compare_detail}; "
        f"gfrun_returncode={gfrun_returncode}; "
        f"run_status={run_status}, chk_status={chk_status}, loss={loss}; "
        f"stdout_last={stdout_preview}; stderr_last={stderr_preview}"
    )


def print_one_result(idx, total, item):
    print(f"[{idx}/{total}] {item['elf_path']}")
    print(f"  最终结果: {item['final_result']}")
    print(f"  原因    : {item['reason']}")
    print(f"  run_verify_returncode: {item['proc_returncode']}")
    print(f"  gfrun_returncode     : {item['gfrun_returncode']}")
    print("")


def print_summary(results):
    counter = {k: 0 for k in FINAL_RESULT_PATTERNS}
    unknown_counter = {}

    for item in results:
        fr = item["final_result"]
        if fr in counter:
            counter[fr] += 1
        else:
            unknown_counter[fr] = unknown_counter.get(fr, 0) + 1

    total = len(results)
    passed = 0
    failed_or_uncertain = 0

    pass_set = {
        "qemu未跑过，gfrun结果校验通过",
        "qemu结果校验未通过，gfrun结果校验通过",
        "qemu结果校验通过，gfrun结果校验通过",
    }

    for item in results:
        if item["final_result"] in pass_set:
            passed += 1
        else:
            failed_or_uncertain += 1

    print("========== 汇总统计 ==========")
    print(f"总文件数: {total}")
    print(f"通过数  : {passed}")
    print(f"未通过/不确定数: {failed_or_uncertain}")
    print("")

    print("---- 6种结果详细计数 ----")
    for k in FINAL_RESULT_PATTERNS:
        print(f"{k}: {counter[k]}")

    if unknown_counter:
        print("")
        print("---- 其他未识别结果计数 ----")
        for k, v in sorted(unknown_counter.items(), key=lambda x: x[0]):
            print(f"{k}: {v}")


def main():
    parser = argparse.ArgumentParser(
        description="Batch run run_verify.py on all elf files under a directory"
    )
    parser.add_argument("-p", "--program", required=True, help="gfrun程序路径")
    parser.add_argument("-l", "--list-dir", required=True, help="包含多个elf文件的目录路径")
    parser.add_argument("-d", "--dir", required=True, help="JanusCoreBench库路径")
    parser.add_argument("--dtype", default="FP32", help="数据格式，默认 FP32")
    parser.add_argument("--j", type=int, default=20, help="并发进程数，默认20")
    args = parser.parse_args()

    gfrun_path = resolve_path(args.program)
    elf_root = resolve_path(args.list_dir)
    janus_root = resolve_path(args.dir)
    run_verify_script = find_run_verify_script()

    if not gfrun_path.exists():
        raise FileNotFoundError(f"gfrun not found: {gfrun_path}")
    if not elf_root.exists() or not elf_root.is_dir():
        raise FileNotFoundError(f"elf root dir not found or not a directory: {elf_root}")
    if not janus_root.exists() or not janus_root.is_dir():
        raise FileNotFoundError(f"JanusCoreBench dir not found or not a directory: {janus_root}")
    if args.j <= 0:
        raise ValueError("--j must be > 0")

    elf_files = find_all_elf_files(elf_root)
    if not elf_files:
        print(f"未在目录中找到任何 .elf 文件: {elf_root}")
        return

    print("========== 任务信息 ==========")
    print(f"run_verify.py : {run_verify_script}")
    print(f"gfrun程序路径 : {gfrun_path}")
    print(f"elf根目录     : {elf_root}")
    print(f"Janus库路径   : {janus_root}")
    print(f"dtype         : {args.dtype} [default=FP32]")
    print(f"并发进程数    : {args.j}")
    print(f"elf文件总数   : {len(elf_files)}")
    print("")

    results = [None] * len(elf_files)
    future_to_index = {}

    with ProcessPoolExecutor(max_workers=args.j) as executor:
        for i, elf_path in enumerate(elf_files):
            future = executor.submit(
                run_one,
                str(run_verify_script),
                str(gfrun_path),
                str(elf_path),
                str(janus_root),
                str(args.dtype),
            )
            future_to_index[future] = i

        done_count = 0
        for future in as_completed(future_to_index):
            idx = future_to_index[future]
            elf_path = elf_files[idx]

            try:
                item = future.result()
            except Exception as e:
                item = {
                    "elf_path": str(elf_path),
                    "proc_returncode": -1,
                    "gfrun_returncode": None,
                    "final_result": "未解析到最终结果",
                    "compare_detail": None,
                    "mismatch_detail": None,
                    "run_status": None,
                    "chk_status": None,
                    "loss": None,
                    "reason": f"执行异常: {repr(e)}",
                    "stdout": "",
                    "stderr": "",
                }

            results[idx] = item
            done_count += 1
            print_one_result(done_count, len(elf_files), item)

    print_summary(results)


if __name__ == "__main__":
    main()
