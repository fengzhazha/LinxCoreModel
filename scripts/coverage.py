#!/usr/bin/env python3

'''
编译：
python3 build.py -c -OO0 --enable_coverage

清理旧覆盖率数据
python3 scripts/coverage.py clean \
  --build-dir build \
  --output-dir build/coverage

测例执行:本脚本不包含执行部分

生成 FULL 报告
python3 scripts/coverage.py report \
  --mode FULL \
  --build-dir build \
  --source-dir . \
  --output-dir build/coverage

生成 DIFF 报告
python3 scripts/coverage.py report \
  --mode DIFF \
  --build-dir build \
  --source-dir . \
  --output-dir build/coverage \
  --base-ref origin/main \
  --diff-threshold 80

'''

import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
from typing import Dict, Set, Tuple, List


def run_cmd(cmd, cwd=None, env=None, check=True, capture=False):
    print(f"[CMD] {' '.join(cmd)}")
    return subprocess.run(
        cmd,
        cwd=cwd,
        env=env,
        check=check,
        text=True,
        capture_output=capture,
    )


def mkdir_p(path: str):
    pathlib.Path(path).mkdir(parents=True, exist_ok=True)


def remove_files_by_suffix(root: str, suffix: str):
    root_path = pathlib.Path(root)
    if not root_path.exists():
        return
    for p in root_path.rglob(f"*{suffix}"):
        try:
            p.unlink()
            print(f"[DEL] {p}")
        except FileNotFoundError:
            pass


def find_gcovr_cmd() -> List[str]:
    gcovr_path = shutil.which("gcovr")
    if gcovr_path:
        return [gcovr_path]

    try:
        subprocess.run(
            [sys.executable, "-m", "gcovr", "--version"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        return [sys.executable, "-m", "gcovr"]
    except Exception:
        pass

    raise FileNotFoundError(
        "gcovr not found.\n"
        "Please install it with one of the following:\n"
        "  python3 -m pip install gcovr\n"
        "or ensure 'gcovr' is in PATH."
    )


def clean_coverage_data(build_dir: str, output_dir: str):
    print("[INFO] Cleaning old coverage data...")
    remove_files_by_suffix(build_dir, ".gcda")
    if os.path.exists(output_dir):
        shutil.rmtree(output_dir)
        print(f"[DEL] {output_dir}")
    mkdir_p(output_dir)


def generate_gcovr_reports(build_dir: str, source_dir: str, output_dir: str, full_threshold: float = None):
    mkdir_p(output_dir)

    gcovr_cmd = find_gcovr_cmd()

    json_report = os.path.join(output_dir, "coverage.json")
    html_report = os.path.join(output_dir, "coverage.html")
    xml_report = os.path.join(output_dir, "coverage.xml")
    txt_report = os.path.join(output_dir, "coverage.txt")

    base_cmd = gcovr_cmd + [
        "--root", source_dir,
        "--object-directory", build_dir,
        "--print-summary",
        "--gcov-ignore-parse-errors=suspicious_hits.warn_once_per_file",
    ]

    if full_threshold is not None:
        base_cmd += ["--fail-under-line", str(full_threshold)]

    run_cmd(base_cmd + ["--json", "--json-pretty", "-o", json_report], cwd=build_dir)
    run_cmd(base_cmd + ["--html-details", "-o", html_report], cwd=build_dir)
    # run_cmd(base_cmd + ["--xml", "-o", xml_report], cwd=build_dir)

    # result = run_cmd(base_cmd, cwd=build_dir, capture=True)
    # with open(txt_report, "w", encoding="utf-8") as f:
    #     f.write(result.stdout)

    print("[INFO] Reports generated:")
    print(f"  JSON: {json_report}")
    print(f"  HTML: {html_report}")
    # print(f"  XML : {xml_report}")
    # print(f"  TXT : {txt_report}")

    return json_report


def git_diff_changed_lines(source_dir: str, base_ref: str) -> Dict[str, Set[int]]:
    cmd = ["git", "diff", "--unified=0", f"{base_ref}...HEAD", "--", "."]
    res = run_cmd(cmd, cwd=source_dir, capture=True)
    diff_text = res.stdout

    file_changes: Dict[str, Set[int]] = {}
    current_file = None

    diff_re = re.compile(r"^\+\+\+ b/(.+)$")
    hunk_re = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")

    for line in diff_text.splitlines():
        m = diff_re.match(line)
        if m:
            current_file = m.group(1)
            file_changes.setdefault(current_file, set())
            continue

        m = hunk_re.match(line)
        if m and current_file:
            start = int(m.group(1))
            count = int(m.group(2) or "1")
            if count > 0:
                for lineno in range(start, start + count):
                    file_changes[current_file].add(lineno)

    return file_changes


def load_gcovr_json(json_report: str, source_dir: str) -> Dict[str, Dict[int, int]]:
    with open(json_report, "r", encoding="utf-8") as f:
        data = json.load(f)

    result: Dict[str, Dict[int, int]] = {}
    files = data.get("files", [])
    source_dir = os.path.abspath(source_dir)

    for fobj in files:
        file_path = fobj["file"]
        if os.path.isabs(file_path):
            rel_path = os.path.relpath(file_path, source_dir)
        else:
            rel_path = file_path

        line_map: Dict[int, int] = {}
        for line in fobj.get("lines", []):
            lineno = line.get("line_number")
            count = line.get("count", 0)
            if lineno is not None:
                line_map[lineno] = count

        result[rel_path] = line_map

    return result


def check_diff_coverage(source_dir: str, json_report: str, base_ref: str, min_rate: float) -> None:
    changed = git_diff_changed_lines(source_dir, base_ref)
    covered_data = load_gcovr_json(json_report, source_dir)

    total_changed_lines = 0
    covered_changed_lines = 0
    uncovered_details: List[Tuple[str, int]] = []

    for rel_file, changed_lines in changed.items():
        if rel_file not in covered_data:
            for lineno in sorted(changed_lines):
                total_changed_lines += 1
                uncovered_details.append((rel_file, lineno))
            continue

        line_cov = covered_data[rel_file]
        for lineno in sorted(changed_lines):
            total_changed_lines += 1
            if line_cov.get(lineno, 0) > 0:
                covered_changed_lines += 1
            else:
                uncovered_details.append((rel_file, lineno))

    if total_changed_lines == 0:
        print("[INFO] No changed lines found in diff. Diff coverage check passed.")
        return

    rate = covered_changed_lines / total_changed_lines * 100.0
    print(f"[INFO] Diff coverage: {covered_changed_lines}/{total_changed_lines} = {rate:.2f}%")

    if uncovered_details:
        print("[INFO] Uncovered changed lines:")
        for rel_file, lineno in uncovered_details:
            print(f"  {rel_file}:{lineno}")

    if rate < min_rate:
        print(f"[ERROR] Diff coverage {rate:.2f}% is below threshold {min_rate:.2f}%")
        sys.exit(2)


def do_clean(args):
    build_dir = os.path.abspath(args.build_dir)
    output_dir = os.path.abspath(args.output_dir)
    clean_coverage_data(build_dir, output_dir)
    print("[INFO] Coverage data cleaned.")


def do_report(args):
    build_dir = os.path.abspath(args.build_dir)
    source_dir = os.path.abspath(args.source_dir)
    output_dir = os.path.abspath(args.output_dir)

    json_report = generate_gcovr_reports(
        build_dir=build_dir,
        source_dir=source_dir,
        output_dir=output_dir,
        full_threshold=args.full_threshold,
    )

    if args.mode == "DIFF":
        check_diff_coverage(
            source_dir=source_dir,
            json_report=json_report,
            base_ref=args.base_ref,
            min_rate=args.diff_threshold,
        )

    print("[INFO] Coverage report completed suaccelssfully.")


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="action", required=True)

    clean_parser = subparsers.add_parser("clean", help="Clean old .gcda files and report directory")
    clean_parser.add_argument("--build-dir", required=True)
    clean_parser.add_argument("--output-dir", required=True)
    clean_parser.set_defaults(func=do_clean)

    report_parser = subparsers.add_parser("report", help="Collect coverage data and generate reports")
    report_parser.add_argument("--mode", choices=["FULL", "DIFF"], required=True)
    report_parser.add_argument("--build-dir", required=True)
    report_parser.add_argument("--source-dir", required=True)
    report_parser.add_argument("--output-dir", required=True)
    report_parser.add_argument(
        "--base-ref",
        default="origin/main",
        help="Git base ref for DIFF mode, e.g. origin/main",
    )
    report_parser.add_argument(
        "--diff-threshold",
        type=float,
        default=80.0,
        help="Required diff line coverage percentage",
    )
    report_parser.add_argument(
        "--full-threshold",
        type=float,
        default=None,
        help="Optional full coverage threshold",
    )
    report_parser.set_defaults(func=do_report)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
