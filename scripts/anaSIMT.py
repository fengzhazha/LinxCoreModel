#!/usr/bin/python3
import os
import sys
import argparse
import plotly.graph_objs as go
from typing import List,Dict

inputBuffer:dict    = dict()
sample_interval:int = 0
left_threshold       = []
right_threshold      = []
info_list            = []
color_list           = []

def createDir(folder):
    if not os.path.isdir(folder):
        os.makedirs(folder)

def getLogs(path:str, outdir:str, readNum):
    print('inputpath: ',path)
    print('outdir: ',outdir)
    file_name, file_ext = os.path.splitext(path)
    htmloutput = os.path.join(outdir, os.path.basename(path).replace(file_ext,'') + '.html')
    print('htmloutput ', htmloutput)
    global inputBuffer
    global sample_interval
    global left_threshold
    global right_threshold
    global info_list
    global color_list
    left_threshold  = [0x111d0, 0x111ea, 0x11216, 0x11234, 0x1123e, 0x1125c, 0x11290]
    right_threshold = [0x111e0, 0x1120e, 0x1122a, 0x11234, 0x11254, 0x11270, 0x1129a]
    info_list       = ['Oi li mi 的buffer分配及初始化, Qi的Buffer 分配及加载', 
                        'Si 的分配及运算', 
                        '[softmax(si)*vi]:softmax 计算及中间结果buffer申请', 
                        '[softmax(si)*vi]:申请中间结果buffer Pi*Vi', 
                        '[softmax(si)*vi]:运算pi*vj', 
                        '[softmax(si)*vi]:释放中间结果buffer:Pi, Pi*Vi 结果,mj-1',
                        '[softmax(si)*vi]:释放buffer: Oi li mi']
                    #   绿         蓝         灰色         紫色      红色       棕色        橙色
    color_list      = ['#00FF00', '#0000FF', '#8F8FBD', '#9F5F9F', '#FF00FF', '#A67D3D', '#FF7F00']

    with open(path, 'r', encoding='utf-8') as f:
        count = 0
        getLog = False
        color_idx = 0
        for line in f.readlines():
            srcCnt = 0
            # Filter input.
            if getLog == False:
                if 'FVEC' not in line:
                    continue
                else:
                    getLog = True
                    bpc = int(line.split(' ')[2], 16)
                    for i in range(0, len(left_threshold)):
                        if bpc >= left_threshold[i] and bpc <= right_threshold[i]:
                            color_idx = i
                            break
                    continue
            else:
                if not line.startswith('M'):
                    if 'FVEC' not in line:
                        getLog = False
                        continue
                    else:
                        bpc = int(line.split(' ')[2], 16)
                        for i in range(0, len(left_threshold)):
                            if bpc >= left_threshold[i] and bpc <= left_threshold[i]:
                                color_idx = i
                                break
            if count % sample_interval != 0:
                count += 1
                continue
            array = line.split('|')
            # print(array)
            cyc = int(array[1].split(':')[-1])
            buffer_size = int(array[2].split(':')[-1], 16)
            inputBuffer[cyc] = [buffer_size, color_idx]
            # readNum = 100
            if readNum != 0 and count >= readNum:
                htmloutput = htmloutput.replace('.html', (str(readNum) + '.html'))
                break
            count += 1
    print('Parse log suaccelssful')
    x_array = []
    y_array = []
    color_array = []
    for key, value in inputBuffer.items():
        x_array.append(key)
        y_array.append(value[0])
        color_array.append(color_list[value[1]])
    # 定义折线图
    # trace = go.Scatter(x=x_array, y=y_array, mode='lines')
    trace = go.Scatter(x=x_array, y=y_array, mode='markers', marker=dict(color=color_array), showlegend=False)
    data = [trace]
    for i in range(0, len(info_list)):
        trace_label = go.Scatter(x=[None],y=[None], mode='markers', marker=dict(color=color_list[i],size=10,opacity=0.8),showlegend=True,name=info_list[i])
        data.append(trace_label)

    # 定义图表布局
    layout = go.Layout(title='Buffer_Size with Cycle', xaxis=dict(title='Cycle'), yaxis=dict(title='Buffer_Size',tickformat='x'))

    # 绘制图表
    fig = go.Figure(data=data, layout=layout)

    # 输出到html文件
    print('Start Html Generation')
    print('Html output: ', htmloutput)
    fig.write_html(htmloutput)    
    print('Html Generation Done')
    return




if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Tensor Visualization Analysis.')
    parser.add_argument('-o','--outdir',help='out directory', required=False)
    parser.add_argument('-i','--inputdir',help='input file.', required=False)
    parser.add_argument('-n','--number',help='number of read lines', required=False)
    parser.add_argument('-s','--sample',help='sampling interval', required=False)
    # global sample_interval
    args = parser.parse_args()
    print(args.outdir)
    createDir(args.outdir)
    outDir = {}
    
    if args.number == None:
        num = 0
    else:
        num = int(args.number)
    if args.sample == None:
        sample_interval = 1
    else:
        sample_interval = int(args.sample)
    if os.path.exists(os.path.isfile(args.inputdir)) and (os.path.isfile(args.inputdir)):
        file = os.path.basename(args.inputdir)
        getLogs(args.inputdir, args.outdir, num)
    else :
        print('Error Input')
