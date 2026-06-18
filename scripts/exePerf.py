#!/usr/bin/env python3
"""
perf执行脚本
使用方法：python exePerf.py 执行命令(如./gfsim -f a.elf)
脚本执行完后会打印执行时间占比前十的函数的名字，并生成svg图于脚本执行路径下
本脚本默认提供的 FlameGraph 工具位于 10.145.6.44 服务器上，若无法访问则需自行提供
若生成的svg图无法正确显示函数名或调用关系,可以尝试将编译等级改为O0并重跑
"""
import re
import argparse
import subprocess
import shutil
import os
import sys

ANSI_RE = re.compile(r'\x1B$[0-9;]*[mK]')

def strip_ansi(s: str) -> str:
    return ANSI_RE.sub('', s)

def parse_perf_tree(lines):
    # 匹配形如:
    # " |--84.13%--RunSimulation"
    # " | --83.26%--JCore::SimSys::step"
    pattern = re.compile(r'^(?P<indent>[ \|]*)--(?P<pct>\d+(?:\.\d+)?)%--(?P<name>.+?)\s*$')
    entries = []
    for idx, raw in enumerate(lines):
        line = strip_ansi(raw.rstrip('\n')).replace('\r', '')
        m = pattern.match(line)
        if not m:
            continue
        indent = m.group('indent') or ''
        pct = float(m.group('pct'))
        name = m.group('name').strip()
        depth = indent.count('|')
        entries.append((idx, depth, pct, name))
    return entries

def collect_children(entries, target_name, first_only=True):
    target_indices = [i for i, (_, _, _, name) in enumerate(entries) if name == target_name]
    if not target_indices:
        raise SystemExit(f'Target function not found in the file: {target_name}')
    ti = target_indices[0] if first_only else target_indices[-1]
    start_line_index, target_depth, target_pct, _ = entries[ti]

    level1, level2, level3 = [], [], []
    for (line_idx, depth, pct, name) in entries[ti + 1:]:
        if depth <= target_depth:
            break
        if depth == target_depth + 1:
            level1.append((pct, name))
        elif depth == target_depth + 2:
            level2.append((pct, name))
        elif depth == target_depth + 3:
            level3.append((pct, name))

    # 按占比降序并仅保留前 10 个
    level1.sort(key=lambda x: x[0], reverse=True)
    level2.sort(key=lambda x: x[0], reverse=True)
    level3.sort(key=lambda x: x[0], reverse=True)
    level1 = level1[:10]
    level2 = level2[:10]
    level3 = level3[:10]

    return {
        'target_line_index': start_line_index,
        'target_depth': target_depth,
        'target_percent': target_pct,
        'level1': level1,
        'level2': level2,
        'level3': level3,
    }

def write_output(result, out_path, target_name):
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(
            f'Target function: {target_name}(Line number {result["target_line_index"]}, '
            f'Depth {result["target_depth"]}, Proportion {result["target_percent"]:.2f}%)'
        )
        f.write('\nFirst-level subfunction:\n')
        if result['level1']:
            for i, (pct, name) in enumerate(result['level1'], 1):
                f.write(f'Top{i}   {name}   {pct:.2f}%\n')
        else:
            f.write('(none)\n')

        f.write('\nSecond-level subfunction:\n')
        if result['level2']:
            for i, (pct, name) in enumerate(result['level2'], 1):
                f.write(f'Top{i}   {name}   {pct:.2f}%\n')
        else:
            f.write('(none)\n')

        f.write('\nThird-level subfunction:\n')
        if result['level3']:
            for i, (pct, name) in enumerate(result['level3'], 1):
                f.write(f'Top{i}   {name}   {pct:.2f}%\n')
        else:
            f.write('(none)\n')

def print_output(result, target_name):
    print(f'Target function: {target_name}(Line number {result["target_line_index"]}, '
          f'Depth {result["target_depth"]}, Proportion {result["target_percent"]:.2f}%)')
    print('\nFirst-level subfunction:')
    if result['level1']:
        for i, (pct, name) in enumerate(result['level1'], 1):
            print(f'Top{i}   {name}   {pct:.2f}%')
    else:
        print('(none)\n')

    print('\nSecond-level subfunction:')
    if result['level2']:
        for i, (pct, name) in enumerate(result['level2'], 1):
            print(f'Top{i}   {name}   {pct:.2f}%')
    else:
        print('(none)\n')

    print('\nThird-level subfunction:')
    if result['level3']:
        for i, (pct, name) in enumerate(result['level3'], 1):
            print(f'Top{i}   {name}   {pct:.2f}%')
    else:
        print('(none)\n')

def run_cmd(cmd, stdout_path=None, merge_stderr=False):
    if stdout_path:
        with open(stdout_path, 'wb') as f:
            subprocess.run(cmd, check=True, stdout=f,
                           stderr=(subprocess.STDOUT if merge_stderr else None))
    else:
        subprocess.run(cmd, check=True)

def run_perf_pipeline(program_cmd, freq, perf_data='perf.data',
                      perf_report='perf_report.txt',
                      perf_unfold='perf.unfold',
                      perf_folded='perf.folded',
                      perf_svg='perf.svg',
                      stackcollapse_path=None,
                      flamegraph_path=None):
    # 确认 perf 是否可用
    if not shutil.which('perf'):
        raise SystemExit('Error: perf not found. Please ensure it is installed and in your PATH.')

    # 若未指定，则在 PATH 中查找 FlameGraph
    # 本脚本默认提供的 FlameGraph 工具位于 10.145.6.44 服务器上，若无法访问则需自行提供
    stackcollapse = stackcollapse_path or shutil.which('/home/ganzuwei/FlameGraph-master/stackcollapse-perf.pl')
    flamegraph = flamegraph_path or shutil.which('/home/ganzuwei/FlameGraph-master/flamegraph.pl')
    if stackcollapse is None:
        print('Warning: stackcollapse-perf.pl was not found (path can be specified with --stackcollapse), so the stack collapsing step for flame graph generation will be skipped.')
    if flamegraph is None:
        print('Warning: flamegraph.pl not found (path can be specified with --flamegraph), generation of perf.svg will be skipped.')

    # 清理旧文件
    for p in [perf_data, perf_report, perf_unfold, perf_folded, perf_svg]:
        try:
            if p and os.path.exists(p):
                os.remove(p)
        except Exception:
            pass

    # perf record
    cmd_record = ['perf', 'record', '-o', perf_data, '-F', str(freq), '-g', '--'] + program_cmd
    print('Run: ', ' '.join(cmd_record))
    run_cmd(cmd_record)

    # perf report -> perf_report.txt
    cmd_report = ['perf', 'report', '--stdio', '--sort', 'comm,dso,symbol']
    print('Run: ', ' '.join(cmd_report), f'> {perf_report}')
    run_cmd(cmd_report, stdout_path=perf_report, merge_stderr=True)

    # perf script -> perf.unfold
    cmd_script = ['perf', 'script', '-i', perf_data]
    print('Run: ', ' '.join(cmd_script), f'&> {perf_unfold}')
    run_cmd(cmd_script, stdout_path=perf_unfold, merge_stderr=True)

    # stackcollapse-perf.pl -> perf.folded
    if stackcollapse:
        cmd_stack = [stackcollapse, perf_unfold]
        print('Run: ', ' '.join(cmd_stack), f'&> {perf_folded}')
        run_cmd(cmd_stack, stdout_path=perf_folded, merge_stderr=True)
    else:
        print('Skip the stackcollapse-perf.pl step (script not found).')

    # flamegraph.pl -> perf.svg
    if flamegraph and os.path.exists(perf_folded):
        cmd_fg = [flamegraph, perf_folded]
        print('Run: ', ' '.join(cmd_fg), f'&> {perf_svg}')
        run_cmd(cmd_fg, stdout_path=perf_svg, merge_stderr=True)
    else:
        if not flamegraph:
            print('Skipping the flamegraph.pl step (script not found).')
        elif not os.path.exists(perf_folded):
            print('Skip the flamegraph.pl step (perf.folded does not exist).')

def main():
    parser = argparse.ArgumentParser(
        description='对程序执行 perf 并解析 perf_report.txt 生成 analyzer.txt'
    )
    parser.add_argument('-i', '--input', default='perf_report.txt',
                        help='perf 报告 txt 文件路径（默认 perf_report.txt）')
    parser.add_argument('-o', '--output',
                        help='输出结果 txt 文件路径（默认打印到屏幕上）')
    parser.add_argument('--target', default='JCore::SimSys::step',
                        help='目标函数名（默认 JCore::SimSys::step）')
    parser.add_argument('--freq', type=int, default=99,
                        help='perf record 采样频率（默认 99）')
    parser.add_argument('--stackcollapse', default=None,
                        help='stackcollapse-perf.pl 脚本路径（默认查 /home/ganzuwei/FlameGraph-master/stackcollapse-perf.pl')
    parser.add_argument('--flamegraph', default=None,
                        help='flamegraph.pl 脚本路径（默认查 /home/ganzuwei/FlameGraph-master/flamegraph.pl')
    parser.add_argument('--skip-perf', action='store_true',
                        help='跳过 perf 执行，只解析已有的 perf_report.txt')

    # 捕获位置参数作为待执行程序命令
    parser.add_argument('program', nargs=argparse.REMAINDER,
                        help='要执行的程序命令及参数，例如：gfsim -f test.txt')

    args = parser.parse_args()

    # 若提供了程序命令并未选择 skip，则执行 perf 管线
    if args.program and not args.skip_perf:
        # 去掉可能的分隔符 --（某些用法会插入），并保留后续参数
        program_cmd = [p for p in args.program if p != '--']
        if not program_cmd:
            print('Warning: No valid program command detected, skipping perf execution.')
        else:
            try:
                run_perf_pipeline(
                    program_cmd=program_cmd,
                    freq=args.freq,
                    perf_data='perf.data',
                    perf_report=args.input,
                    perf_unfold='perf.unfold',
                    perf_folded='perf.folded',
                    perf_svg='perf.svg',
                    stackcollapse_path=args.stackcollapse,
                    flamegraph_path=args.flamegraph,
                )
            except subprocess.CalledProcessError as e:
                raise SystemExit(f'perf step execution failed, exit code {e.returncode}：{e}')

    # 读取并解析 perf_report.txt
    try:
        with open(args.input, 'r', encoding='utf-8', errors='ignore') as f:
            lines = [line.rstrip('\n') for line in f]
    except FileNotFoundError:
        raise SystemExit(f'Input file not found: {args.input}. To generate automatically, please pass the program command, such as: python jiaoben.py gfsim -f test.txt')

    entries = parse_perf_tree(lines)
    result = collect_children(entries, args.target, first_only=True)
    print_output(result, args.target)
    # 也可以输出到给定的文件中
    # write_output(result, args.output, args.target)
    # print(f'The analysis is complete and the following information has been output: {args.output}')

if __name__ == '__main__':
    main()
