"""
Draw Histogram
"""
import matplotlib.pyplot as plt
import matplotlib
import numpy as np
import os
import argparse
import csv
from matplotlib.ticker import FuncFormatter
import plotly.graph_objects as go
import plotly.offline as pyo
import copy
from bs4 import BeautifulSoup, Comment
import plotly.io as pio

argParser = argparse.ArgumentParser(description="run gfrun creat log")
argParser.add_argument("-f", "--file", metavar="ckpt dir", type=str, help="ckpt dir")
argParser.add_argument("-m", "--mode", metavar="histogram mode", type=int, help="histogram mode")
args = argParser.parse_args()

class DrawHistogram:
    def __init__(self, file_path, mode, soup):
        self.rate_layer = 0
        self.file_path = file_path
        self.mode = mode
        self.file_name = ''
        self.csv_dict = {}
        self.draw_dict = {}
        self.layer_name = []
        self.file_key_name = ''
        self.run_all = False
        self.soup = soup

    def traverse_file(self):
        for root, dirs, files in os.walk(self.file_path):
            for file_name in files:
                if self.file_key_name in file_name:
                    if file_name.split("-")[0] not in self.csv_dict:
                        self.csv_dict[file_name.split("-")[0]] = [os.path.join(root, file_name)]
                    else:
                        self.csv_dict[file_name.split("-")[0]].append(os.path.join(root, file_name))

    def interval_judge(self, row):
        if row[0] in self.layer_name:
            return self.layer_name.index(row[0])
        else:
            return len(self.layer_name) - 1
    
    def parse_file(self, file_path, draw_dict):
        if self.mode == 5:
            with open(file_path, 'r', encoding='UTF8') as f:
                origin_contents = f.readlines()
                for index, row in enumerate(origin_contents):
                    if "total Block number with store" in row:
                        draw_dict[int(row.split(",")[0].split("=")[-1].strip())] = int(row.split(",")[1].split("=")[-1].strip())
                        break
        else:
            with open(file_path) as f:
                csv_content = csv.reader(f)
                headers = next(csv_content)
                count_list = []

                if self.mode == 0 or self.mode == 1:
                    for i in range(self.rate_layer):
                        count_list.append(0)
                    for row in csv_content:
                        count_list[self.interval_judge(row)] += int(row[1])
                    return count_list

                # Alu, lda, bru, sta
                elif self.mode == 2:
                    for row in csv_content:
                        count_list.append([int(row[1]), int(row[2])])
                    return count_list
                
                elif self.mode == 4:
                    for row in csv_content:
                        if int(row[0]) not in draw_dict:
                            draw_dict[int(row[0])] = int(row[1])
                        else:
                            draw_dict[int(row[0])] += int(row[1])

    def generate_histogram(self):
        for file_name, file_paths in self.csv_dict.items():
            if len(file_paths) == 0:
                continue
            
            draw_dict = {}

            if self.mode == 0 or self.mode == 1:
                count_matrix = []
                for csv_path in file_paths:
                    count_list = self.parse_file(csv_path, draw_dict)
                    count_matrix.append(count_list)

                sum_list = [0 for i in range(self.rate_layer)]

                for layer_num in range(self.rate_layer):
                    for ckpt_index in range(len(count_matrix)):
                        sum_list[layer_num] += count_matrix[ckpt_index][layer_num]
            elif self.mode == 2:
                count_matrix = [[0, 0] for i in range(self.rate_layer)]
                for csv_path in file_paths:
                    count_list = self.parse_file(csv_path, draw_dict)
                    for i in range(self.rate_layer):
                        count_matrix[i][0] += count_list[i][0]
                        count_matrix[i][1] += count_list[i][1]
                sum_list = []
                for i in range(self.rate_layer):
                    sum_list.append(round(count_matrix[i][1] / count_matrix[i][0], 2))
            elif self.mode == 4:
                for file_path in file_paths:
                    self.parse_file(file_path, draw_dict)
            elif self.mode == 5:
                for file_path in file_paths:
                    self.parse_file(file_path, draw_dict)

            if self.mode == 0 or self.mode == 1:
                result_list = []
                for index, count in enumerate(sum_list):
                    result_list.append(round(count / sum(sum_list), 5))
                self.draw_dict[file_name] = result_list
            elif self.mode == 2:
                self.draw_dict[file_name] = sum_list
            elif self.mode == 4:
                draw_dict = dict(sorted(draw_dict.items()))
                rate_draw_dict = {}
                new_rate_draw_dict = {}
                count_sum = sum(draw_dict.values())
                accumulate_before_rate = 0
                num_count = 0
                
                for key, count in draw_dict.items():
                    rate_draw_dict[key] = round(count / count_sum, 10) + accumulate_before_rate
                    accumulate_before_rate = rate_draw_dict[key]

                for key, value in rate_draw_dict.items():
                    num_count += 1
                    if 0 < num_count <= 10:
                        new_rate_draw_dict[list(rate_draw_dict.keys())[num_count-1]] = list(rate_draw_dict.values())[num_count-1]
                    if 10 < num_count <= 100 and num_count % 10 == 0:
                        new_rate_draw_dict[list(rate_draw_dict.keys())[num_count-1]] = list(rate_draw_dict.values())[num_count-1]
                    if 100 < num_count <= 300 and num_count % 25 == 0:
                        new_rate_draw_dict[list(rate_draw_dict.keys())[num_count-1]] = list(rate_draw_dict.values())[num_count-1]
                    if 300 < num_count <= 1000 and num_count % 100 == 0:
                        new_rate_draw_dict[list(rate_draw_dict.keys())[num_count-1]] = list(rate_draw_dict.values())[num_count-1]   
                    if 1000 < num_count <= 1500 and num_count % 100 == 0:
                        new_rate_draw_dict[list(rate_draw_dict.keys())[num_count-1]] = list(rate_draw_dict.values())[num_count-1]
                    if 1500 < num_count <= 10000 and num_count % 1000 == 0:
                        new_rate_draw_dict[list(rate_draw_dict.keys())[num_count-1]] = list(rate_draw_dict.values())[num_count-1]
                    if 10000 < num_count <= 15000 and num_count % 1000 == 0:
                        new_rate_draw_dict[list(rate_draw_dict.keys())[num_count-1]] = list(rate_draw_dict.values())[num_count-1]
                    if 15000 < num_count <= 100000 and num_count % 10000 == 0:
                        new_rate_draw_dict[list(rate_draw_dict.keys())[num_count-1]] = list(rate_draw_dict.values())[num_count-1]
                    if 100000 < num_count <= 200000 and num_count % 10000 == 0:
                        new_rate_draw_dict[list(rate_draw_dict.keys())[num_count-1]] = list(rate_draw_dict.values())[num_count-1]
                if list(rate_draw_dict.keys())[-1] not in new_rate_draw_dict:
                    new_rate_draw_dict[list(rate_draw_dict.keys())[-1]] = 1
                
                self.draw_dict[file_name] = dict(sorted(new_rate_draw_dict.items()))
            elif self.mode == 5:
                sum_block_num = 0
                sum_store_block_num = 0
                for block_num, store_block_num in draw_dict.items():
                    sum_block_num += block_num
                    sum_store_block_num += store_block_num
                self.draw_dict[file_name] = [round(sum_store_block_num / sum_block_num, 2)]

        self.draw_dict = dict(sorted(self.draw_dict.items()))

        if self.mode == 4:
            self.draw_curve()
        else:
            self.draw_stack_histogram()


    def draw_curve(self):
        # 创建图形对象
        fig = go.Figure()
        fig.update_yaxes(tickformat=".2%")
        fig.update_traces(smoothing=1)
        for file_name, draw_dict in self.draw_dict.items():
            distance_list = list(draw_dict.keys())
            accur_rate_list = list(draw_dict.values())            
            fig.add_trace(go.Scatter(x=[(i + 1) for i in range(len(distance_list))], y=accur_rate_list, mode='lines', name=file_name))
            fig.update_xaxes(tickvals=[(i + 1) for i in range(len(distance_list))], ticktext=[str(i) for i in distance_list])
        # 设置图形布局
        fig.update_layout(margin=dict(b=200), title=self.file_name, xaxis_title='依赖距离', yaxis_title='累计比例')

        html_str = str(pio.to_html(fig))
        if self.soup is None:
            self.soup = BeautifulSoup(html_str, 'html.parser')
        else:
            self.soup.body.append(BeautifulSoup(html_str, 'html.parser'))


    def draw_stack_histogram(self):
        case_num = len(list(self.draw_dict.keys()))
        y_matrix = [[0 for i in range(case_num)] for j in range(self.rate_layer)]
        for lay_num in range(self.rate_layer):
            for slice_num in range(case_num):
                y_matrix[lay_num][slice_num] = list(self.draw_dict.values())[slice_num][lay_num]
  
        x_labels = [subproject_name for subproject_name in list(self.draw_dict.keys())]
        # 创建柱形图

        if self.mode == 0 or self.mode == 1 or self.mode == 5:
            y_name = "比例"
            bar_mode = 'stack'
            if self.mode == 5:
                fig = go.Figure(data=[go.Bar(x=x_labels, y=y_matrix[i]) for i in range(self.rate_layer)])
            else:
                fig = go.Figure(data=[go.Bar(name=self.layer_name[i], x=x_labels, y=y_matrix[i]) for i in range(self.rate_layer)])
            # 显示百分比
            fig.update_yaxes(tickformat=".2%")
            # 添加标题
            fig.update_layout(
                margin=dict(b=150),
                xaxis_title="SPEC2006/2017-0804",
                yaxis_title=y_name,
                title=self.file_name, 
                barmode=bar_mode)
            html_str = str(fig.to_html())

            if self.soup is None:
                self.soup = BeautifulSoup(html_str, 'html.parser')
            else:
                self.soup.body.append(BeautifulSoup(html_str, 'html.parser'))
         
        elif self.mode == 2:
            y_name = "平均Cycle"
            bar_mode = 'group'
            high_data=[go.Bar(name=self.layer_name[i], x=x_labels, y=y_matrix[i]) for i in range(self.rate_layer)]
            high_fig = go.Figure(data=high_data)
            high_fig.update_layout(
                margin=dict(b=150),
                title=self.file_name, 
                xaxis=dict(title="SPEC2006/2017-0804"), 
                yaxis=dict(title=y_name),
                barmode=bar_mode)
         
            high_html_str = str(high_fig.to_html())
            if self.soup is None:
                self.soup = BeautifulSoup(high_html_str, 'html.parser')
            else:
                self.soup.body.append(BeautifulSoup(high_html_str, 'html.parser'))
            
    
        elif self.mode == 3:
            y_name = "平均Cycle"
      
            low_y_list = [34.52, 30.52, 53.47, 14.42, 35.25]
            low_data = [go.Bar(x=self.layer_name, y=low_y_list, width=0.2, marker=dict(
                        color=['blue', 'red', 'green', 'purple', 'orange']))]
            
            low_fig = go.Figure(data=low_data)
            low_fig.update_layout(
                margin=dict(b=150),
                title=self.file_name, 
                xaxis=dict(title="指令类型"), 
                yaxis=dict(title=y_name))
            
            low_html_str = str(low_fig.to_html())
            if self.soup is None:
                self.soup = BeautifulSoup(low_html_str, 'html.parser')
                
            else:
                self.soup.body.append(BeautifulSoup(low_html_str, 'html.parser'))
                
    def exe_by_mode(self):
        if self.mode == 0:
            self.rate_layer = 7
            self.layer_name = ['0x0', '0x1', '0x2', '0x3', '0x4', '0x8', '其他']
            self.file_name = "1 SPECINT子项全部块共享寄存器值统计"
            self.file_key_name = "ur_statistic"
        elif self.mode == 1:
            self.rate_layer = 5
            self.layer_name = ['0', '1', '2', '3', '>=4']
            self.file_name = "2 SPECINT子项全部块共享寄存器写后读的块数统计"
            self.file_key_name =  "log_statistic"
        elif self.mode == 2:
            self.rate_layer = 5
            self.layer_name = ['ALU', 'LDA', 'BRU', 'STA', 'STD']
            self.file_name = "4.1 SPECINT子项每种指令在发射队列等待的平均Cycle数-分开统计"
            self.file_key_name = "inst_issueq_cycle"
        elif self.mode == 3:
            self.layer_name = ['ALU', 'LDA', 'BRU', 'STA', 'STD']
            self.file_name = "4.2 SPECINT子项每种指令在发射队列等待的平均Cycle数-汇总统计"
            self.file_key_name = "inst_issueq_cycle"
        elif self.mode == 4:
            self.rate_layer = 6
            self.file_name = "3 SPECINT子项全部块共享寄存器写后读的间隔块数统计"
            self.file_key_name = ".json_ur_distance.csv"
        elif self.mode == 5:
            self.rate_layer = 1
            self.file_name = "SPECINT子项含Store指令的块占比"
            self.file_key_name = "store_num.log"
        self.traverse_file()
        self.generate_histogram()
        return self.soup

def main():
    soup = None
    task_list = []
    if args.mode == -1:
        task_list = [0, 1, 4, 2, 3]
    else:
        task_list = [args.mode]
  
    for i in task_list:
        instance = DrawHistogram(args.file, i, soup)
        soup = instance.exe_by_mode() 

    if args.mode == -1:
        instance.file_name = "summarize"  

    with open(instance.file_name + ".html", 'w', encoding='utf-8') as f:
        f.write(soup.prettify())

if __name__ == '__main__' :
    main()
