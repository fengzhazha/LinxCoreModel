#!/usr/bin/env python3
"""
性能分析脚本
输入: performance_analyzer.conf配置文件,请按需键入正确的执行命令
功能: 对conf中的配置执行perf和valgrind分析
注意,需要手动修改 FlameGraph_dir(即本地FlameGraph的路径)
若生成的svg图无法正确显示函数名或调用关系,可以尝试将编译等级改为O0并重跑
"""

import os
import sys
import subprocess
import re
import time
from datetime import datetime
import configparser

class PerformanceAnalyzer:
    def __init__(self, config_file):
        self.config_file = config_file
        self.results = {}
        self.report_file = f"performance_output/performance_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        
    def parse_config(self):
        """解析配置文件"""
        config = configparser.ConfigParser()
        config.read(self.config_file)
        
        self.test_cases = {}
        
        for section in config.sections():
            if section.startswith('TEST_CASE'):
                case_id = section.split('_')[-1]
                self.test_cases[case_id] = dict(config[section])
        
        return self.test_cases
    
    def run_operf(self, command, case_id):
        """执行perf性能分析"""
        print(f"Performing perf analysis: {command}")
        
        # 创建输出目录
        operf_dir = f"performance_output/perf_results_{case_id}"
        FlameGraph_dir = f"/home/ganzuwei/FlameGraph-master"
        os.makedirs(operf_dir, exist_ok=True)
        print(f"Perf Output Directory: {os.path.abspath(operf_dir)}\n")

        perf_cmd1 = f"perf record -o {operf_dir}/perf.data -F 99 -g -- {command}"
        perf_cmd2 = f"perf script -i {operf_dir}/perf.data &> {operf_dir}/perf.unfold"
        perf_cmd3 = f"{FlameGraph_dir}/stackcollapse-perf.pl {operf_dir}/perf.unfold &> {operf_dir}/perf.folded"
        perf_cmd4 = f"{FlameGraph_dir}/flamegraph.pl {operf_dir}/perf.folded &> {operf_dir}/perf.svg"
        
        try:
            start_time = time.time()

            result = subprocess.run(perf_cmd1, shell=True, capture_output=True, text=True, timeout=3600)
            result = subprocess.run(perf_cmd2, shell=True, capture_output=True, text=True, timeout=3600)
            result = subprocess.run(perf_cmd3, shell=True, capture_output=True, text=True, timeout=3600)
            result = subprocess.run(perf_cmd4, shell=True, capture_output=True, text=True, timeout=3600)
            
            end_time = time.time()
            
            # 解析operf结果
            # operf_results = self.parse_operf_output(operf_dir)
            
            return {
                'suaccelss': result.returncode == 0,
                'stdout': result.stdout,
                'stderr': result.stderr,
                'execution_time': end_time - start_time,
                # 'operf_data': operf_results
            }
        except subprocess.TimeoutExpired:
            return {
                'suaccelss': False,
                'error': '执行超时',
                'execution_time': 3600
            }
        except Exception as e:
            return {
                'suaccelss': False,
                'error': str(e)
            }

    def parse_operf_output(self, operf_dir):
        """解析operf输出结果"""
        # operf默认在当前目录生成oprofile_data目录
        oprofile_data_dir = os.path.join(operf_dir, "oprofile_data")
        
        if os.path.exists(oprofile_data_dir) and os.path.isdir(oprofile_data_dir):
            try:
                # 使用opreport生成分析报告
                report_cmd = f"opreport -l {oprofile_data_dir}"
                result = subprocess.run(report_cmd, shell=True, 
                                    capture_output=True, text=True)
                
                if result.returncode == 0:
                    return result.stdout
                else:
                    return f"无法解析operf数据: {result.stderr}"
            except Exception as e:
                return f"解析operf数据时出错: {str(e)}"
        else:
            # 检查目录内容以帮助调试
            files_in_dir = os.listdir(operf_dir)
            return f"oprofile_data目录未找到。目录内容: {files_in_dir}"


    
    def run_valgrind(self, command, case_id):
        """执行valgrind内存分析"""
        print(f"Performing valgrind analysis: {command}")
        
        # 创建输出目录
        valgrind_dir = f"performance_output/valgrind_results_{case_id}"
        os.makedirs(valgrind_dir, exist_ok=True)
        print(f"Valgrind Output Directory:{os.path.abspath(valgrind_dir)}")
        
        # 构建valgrind命令
        valgrind_cmd = f"valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --log-file={valgrind_dir}/valgrind_report.txt {command}"
        
        try:
            start_time = time.time()
            result = subprocess.run(valgrind_cmd, shell=True, capture_output=True, text=True, timeout=3600)
            end_time = time.time()
            
            # 解析valgrind结果
            valgrind_results = self.parse_valgrind_output(valgrind_dir)
            
            return {
                'suaccelss': True,
                'stdout': result.stdout,
                'stderr': result.stderr,
                'execution_time': end_time - start_time,
                'valgrind_data': valgrind_results
            }
        except subprocess.TimeoutExpired:
            return {
                'suaccelss': False,
                'error': '执行超时',
                'execution_time': 3600
            }
        except Exception as e:
            return {
                'suaccelss': False,
                'error': str(e)
            }
    
    def parse_valgrind_output(self, valgrind_dir):
        """解析valgrind输出结果"""
        report_file = os.path.join(valgrind_dir, "valgrind_report.txt")
        
        if os.path.exists(report_file):
            with open(report_file, 'r') as f:
                content = f.read()
                
            # 提取关键信息
            leaks_match = re.search(r'definitely lost: (\d+) bytes in (\d+) blocks', content)
            errors_match = re.search(r'ERROR SUMMARY: (\d+) errors', content)
            
            return {
                'content': content,
                'leaks': leaks_match.group(0) if leaks_match else "未检测到内存泄漏",
                'errors': errors_match.group(0) if errors_match else "未检测到错误"
            }
        else:
            return {
                'content': "valgrind报告未找到",
                'leaks': "未知",
                'errors': "未知"
            }
    
    def analyze_test_cases(self):
        """分析所有测试用例"""
        test_cases = self.parse_config()
        
        for case_id, config in test_cases.items():
            print(f"\nStart analyzing test cases {case_id}")
            
            # 获取要执行的命令
            command = config.get('command', '')
            if not command:
                print(f"Test case {case_id} has no specified command, skipping")
                continue
            
            # 执行perf分析
            operf_result = self.run_operf(command, case_id)
            
            # 执行valgrind分析
            valgrind_result = self.run_valgrind(command, case_id)
            
            # 保存结果
            self.results[case_id] = {
                'config': config,
                'operf': operf_result,
                'valgrind': valgrind_result
            }
    
    def generate_report(self):
        """生成性能分析报告"""
        print(f"\nGenerating a Performance Analysis Report: {self.report_file}")
        
        with open(self.report_file, 'w') as f:
            f.write("=" * 60 + "\n")
            f.write("Model Simulation Performance Analysis Report\n")
            f.write("=" * 60 + "\n")
            f.write(f"Generation Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Configuration File: {self.config_file}\n")
            f.write("=" * 60 + "\n\n")
            
            for case_id, result in self.results.items():
                f.write(f"Test Case {case_id}:\n")
                f.write("-" * 40 + "\n")
                f.write(f"Configuration: {result['config']}\n")
                
                # operf结果
                f.write("\noperf performance analysis:\n")
                if result['operf']['suaccelss']:
                    f.write(f"Execution Status: Suaccelss\n")
                    f.write(f"Execution Time: {result['operf']['execution_time']:.2f} Second\n")
                else:
                    f.write(f"Execution Status: Failed\n")
                    if 'error' in result['operf']:
                        f.write(f"Error Message: {result['operf']['error']}\n")
                
                # valgrind结果
                f.write("\nvalgrind performance analysis:\n")
                if result['valgrind']['suaccelss']:
                    f.write(f"Execution Status: Suaccelss\n")
                    f.write(f"Execution Time: {result['valgrind']['execution_time']:.2f} Second\n")
                else:
                    f.write(f"Execution Status: Failed\n")
                    if 'error' in result['valgrind']:
                        f.write(f"Error Message: {result['valgrind']['error']}\n")
                
                f.write("\n" + "=" * 60 + "\n\n")
            
            # 总结
            f.write("Performance Analysis Summary:\n")
            f.write("-" * 40 + "\n")
            suaccelssful_cases = [case_id for case_id, result in self.results.items() 
                               if result['operf']['suaccelss'] and result['valgrind']['suaccelss']]
            failed_cases = [case_id for case_id in self.results.keys() if case_id not in suaccelssful_cases]
            
            f.write(f"Suaccelssfully executed test cases: {len(suaccelssful_cases)}\n")
            f.write(f"Failed Test Cases: {len(failed_cases)}\n")
            
            if failed_cases:
                f.write("Failed test case ID: " + ", ".join(failed_cases) + "\n")
        
        print(f"Report generated: {os.path.abspath(self.report_file)}")

def main():
    if len(sys.argv) != 2:
        print("Usage: python performance_analyzer.py <test_case.conf>")
        sys.exit(1)
    
    config_file = sys.argv[1]
    
    if not os.path.exists(config_file):
        print(f"Error: Configuration file {config_file} does not exist.")
        sys.exit(1)
    analyzer = PerformanceAnalyzer(config_file)
    analyzer.analyze_test_cases()
    analyzer.generate_report()

if __name__ == "__main__":
    main()
