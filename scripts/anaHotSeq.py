#!/usr/bin/python3
import os
import sys
import argparse
import datetime
import time
import fnmatch
import subprocess
import html
import plotly.graph_objs as go
import plotly.offline as pyo
from bs4 import BeautifulSoup
from concurrent.futures import ThreadPoolExecutor
import multiprocessing
from multiprocessing import Pool
from collections import OrderedDict
from typing import List,Dict,Union,Callable,Tuple

gfrun = "/home/k30043533/BlockISA/BlockISA/bin/gfrun"
logFile = ""

def writeLog(log):
    f = open(logFile, "a")
    f.write("[{}] {}".format(datetime.datetime.now(), log))
    f.write("\n")
    f.close()

def createDir(folder):
    if not os.path.isdir(folder):
        os.makedirs(folder)
        writeLog("create dir: {}".format(folder))

def runFuncModel(path:str,blk_num:int,outdir:str,bpcfp_dict):
    # print(path,blk_num,outdir)
    # blk_num = 10000
    
    output = os.path.join(outdir, os.path.basename(path)+'.run.log')
    traceoutput = os.path.join(outdir, os.path.basename(path)+'.bpctrace.log')
    fpoutput = os.path.join(outdir, os.path.basename(path)+'.bpcfp.log')
    anaoutput = os.path.join(outdir, os.path.basename(path)+'.ana.csv')

    # Run Function model
    cmd = f"{gfrun} -t 2 -m {blk_num} -f {path}"
    writeLog(f"running {path}! cmd: {cmd}")
    print('Run function model', os.path.basename(path))
    result = subprocess.run(cmd,shell=True, stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    
    # Process Output
    bpcList:list = list()
    fp_count:dict = dict()
    # Get BPC List And Count BPC Footprint
    with open(traceoutput, 'w') as traceLog:
        traceLog.write("BPC hexadecimal, decimal\n")
        for line in result.stdout.decode().splitlines():
            if line.startswith('B'):
                # print(line.split(" ")[2],", dec:",end='')
                # print(int(line.split(" ")[2], 16))
                if line.split(" ")[2] in fp_count:
                    fp_count[line.split(" ")[2]] += 1
                else:
                    fp_count[line.split(" ")[2]] = 1
                traceLog.write(str(line.split(" ")[2]) + ", " + str(int(line.split(" ")[2], 16))+"\n")
                bpcList.append(int(line.split(" ")[2], 16))

    # Output FootPrint To file
    sorted_d = dict(sorted(fp_count.items(), key=lambda x:x[1], reverse=True))
    with open(fpoutput, 'w') as f:
        f.write('BPC,FootPrint\n')
        for key, value in list(sorted_d.items()):
            outStr:str = str(key) + ',' + str(value) + '\n'
            f.write(outStr)
    bpcfp_dict[os.path.basename(path)] = len(fp_count)

    # Analysis hot sequece by LZW
    print('start analysis hot sequence',anaoutput)
    writeLog("start analysis hot sequence by LZW to {}!".format(anaoutput))
    printCSV(sorted(hotsequence_lzw(bpcList), key=lambda line: line[2] * line[1], reverse=True), anaoutput)

    # Log End
    if result.returncode != 0:
        print('Error function model', os.path.basename(path))
        writeLog("fail {}\nerror {}".format(cmd,result.stderr.decode("utf-8").split('\n')[1]))
    else:
        print('Completed function model', os.path.basename(path))
        writeLog("suaccelssful {}!".format(output))
    return

def getSliceIdToWeight(itemDir):
    sliceIds = []
    with open(os.path.join(itemDir,"simpoints.txt")) as f:
        for line in f.readlines():
            sliceIds.append(int(line.split(' ')[0]))
    weights = []
    with open(os.path.join(itemDir,"simpointWeights.txt")) as f:
        for line in f.readlines():
            weights.append(float(line.split(' ')[0]))
    sliceIdToWeight = {}
    for i in range(len(sliceIds)):
        sliceIdToWeight[sliceIds[i]] = weights[i]
    return sliceIdToWeight

def getSliceIdToBlockNumber(itemDir):
    sliceIdToBlockNumber = {}
    with open(os.path.join(itemDir,"blockNums.txt")) as f:
        for line in f.readlines():
            sliceIdToBlockNumber[int(line.split(' ')[0])] = int(line.split(' ')[1])
    return sliceIdToBlockNumber

def getSliceId(sliceName):
    temp = sliceName.split('-')[2]
    sliceId = temp[0:temp.find('.')]
    return int(sliceId)

def hotsequence_lzw(bpcList:list) -> List[List[int]]:
    # bpc map: key= sequence string, val= index, repeat
    bpcMap:Dict[Tuple,Tuple[int,int]] = dict()

    # print('start lzw')
    # init dict
    for bpc in bpcList:
        if (bpc,) not in bpcMap:
            bpcMap[(bpc,)] = (len(bpcMap), 0)

    compress_result:List[int] = list()

    limit_max_m:bool = True
    max_m:int = -1
    if limit_max_m:
        max_m = 8
    else:
        max_m = len(bpcList)

    curr_seq:tuple = tuple()
    for bpc in bpcList:
        new_seq:tuple = curr_seq + (bpc,)
        if (new_seq in bpcMap) and (len(new_seq) <= max_m):
            curr_seq = new_seq
        else:
            index, count = bpcMap[curr_seq]
            bpcMap[curr_seq] = (index, count + 1)
            compress_result.append(index)
            bpcMap[new_seq] = (len(bpcMap), 0)
            curr_seq = (bpc,)

    if len(curr_seq) != 0:
        index, count = bpcMap[curr_seq]
        compress_result.append(index)
        bpcMap[curr_seq] = (index, count + 1)

    total:int = 0
    result:List[List[int]] = list()
    for key in bpcMap:
        index, count = bpcMap[key]
        if count >= 1:
            #print(bpcMap[key],end=', ')
            #print(key)
            curr_line:List[int] = [index, len(key), count]
            for bpc in key:
                curr_line.append(bpc)
            total += count * len(key)
            result.append(curr_line)

    # decompress
    # swap key value
    decompress_dict = { index:key for key,(index,repeat) in bpcMap.items()}
    seq_list = list()
    decompress_data = list()

    # print('seq')
    for index in compress_result:
        seq_list.append(list(decompress_dict[index]))
        decompress_data.extend(list(decompress_dict[index]))
        #for bpc in decompress_dict[index]:
        #    #print('0x%x' % (bpc),end=',')
        #print('')

    # print('total is %d bpc len is %d' % (total, len(bpcList)))

    assert total == len(bpcList)
    assert decompress_data == bpcList

    #print(compress_result)
    #print('len %d' % (len(compress_result)))

    #exit(-1)

    return result

def printCSV(input:List[List[int]], path:str):
    with open(path,'w') as csvfile:
        csvfile.write('overall coverage,length,repeat,sequence\n')
        for line in input:
            csvline:str = str(line[1]*line[2]) + ',' + str(line[1]) + ',' + str(line[2])
            for val in line[3:]:
                csvline += ',' + str(hex(val))
            csvfile.write(csvline+'\n')

def getCoverSize(path:str):
    total_cover = 0
    with open(path, 'r') as f:
        for line in f:
            if line[0].isdigit():
                # print(line)
                # print(line.split(','))
                if int(line.split(',')[2]) >= 100:
                    total_cover = total_cover + int(line.split(',')[0])
                else:
                    return total_cover
    return total_cover

def drawSum(drawDict:dict,output:str):
    x_array = []
    y_array = []
    outname = output + 'sum.csv'
    outname1 = output + 'sum.html'
    with open(outname, 'w') as f:
        f.write('ckpt_name,loop_blocks,total_blocks,cover_rate\n')
        for key, value in list(drawDict.items()):
            x_array.append(key)
            rate = round(value[0] / value[1], 6)
            # print(key, rate)
            outStr:str = str(key) + ',' + str(value[0]) + ',' + str(value[1]) + ',' + str(rate) + '\n'
            f.write(outStr)
            y_array.append(rate * 100) # cover rate

    trace = go.Bar(
        x = x_array,
        y = y_array,
    )
    data = [trace]
        
    layout = go.Layout(
        title='Loop Cover Blocks Rate',
    )
    fig = go.Figure(data=data, layout=layout)
    pyo.plot(fig, filename=outname1)

def drawFP(drawDict:dict,output:str):
    x_array = []
    y_array = []
    outname = output + 'BPCFootPint.csv'
    outname1 = output + 'BPCFootPint.html'
    with open(outname, 'w') as f:
        f.write('ckpt_name,FootPrint Of BPC\n')
        for key, value in list(drawDict.items()):
            x_array.append(key)
            outStr:str = str(key) + ',' + str(value) + '\n'
            f.write(outStr)
            y_array.append(value) # cover rate
    
    trace = go.Bar(
        x = x_array,
        y = y_array,
    )
    data = [trace]
    layout = go.Layout(
        title='BPC FootPrint',
    )
    fig = go.Figure(data=data, layout=layout)
    pyo.plot(fig, filename=outname1)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Running GFRUN For BPC Analysis.')
    parser.add_argument('-o','--outdir',help='out directory',required=True)
    parser.add_argument('-i','--inputdir',help='input directory.',required=True)
    parser.add_argument('-l','--log',help='log path',required=True)
    parser.add_argument('-n','--blksNum',help='Block numbers',required=False)
    args = parser.parse_args()
    logFile = args.log
    f = open(args.log, "w")
    f.close()
    createDir(args.outdir)
    blkNums = {}
    outDir = {}
    pool = Pool(processes=20)
    bpcfp_dict = multiprocessing.Manager().dict()
    sroted_fp_dict = {}
    count_ckpt = 0

    if (os.path.isfile(args.inputdir)):
        print(args.inputdir)
        file = os.path.basename(args.inputdir)
        print(file)
        print(file.split('-'))
        print()
        print(int(args.blksNum))
        res = {}
        runFuncModel(args.inputdir, int(args.blksNum), args.outdir, res)

    elif os.path.isdir(args.inputdir):
        for root, dirs, files in os.walk(args.inputdir):
            # Get Block Nums And Create Output Directory
            for file in fnmatch.filter(files, 'blockNums.txt'):
                file_name = os.path.join(root, file)
                current_dir = os.path.dirname(file_name)
                folder_name = os.path.basename(current_dir)
                blkNums[folder_name] = {}
                outDir[folder_name] = os.path.join(args.outdir, folder_name)
                createDir(outDir[folder_name])
                print('create log file:' + outDir[folder_name])
                writeLog("creat output file {}!".format(outDir[folder_name]))
                with open(file_name, 'r', encoding='utf-8') as f1:
                    for line in f1.readlines():
                        blkNums[folder_name][int(line.split()[0])] = int(line.split()[1])
                print(folder_name,' blockNums: ',blkNums[folder_name])
                writeLog("get blockNums of {}::\n {}".format(folder_name,blkNums[folder_name]))
                print('------------------------------------')

        for root, dirs, files in os.walk(args.inputdir):
            # Run Function Model And Get Analysis data
            for file in fnmatch.filter(files, '*.json'):
                file_name = os.path.join(root, file)
                current_dir = os.path.dirname(file_name)
                folder_name = os.path.basename(current_dir).split('-')[0]
                benchmark = file.split('-')[0]
                sliceID = int(file.split('-')[-1].replace('.json', ''))
                count_ckpt +=1
                print('scaned: ',file)
                print('Scanned %d ckpt join to run'%count_ckpt)
                print('------------------------------------')
                pool.apply_async(runFuncModel, args=(file_name, blkNums[benchmark][sliceID], outDir[folder_name],bpcfp_dict,))

        pool.close()
        pool.join()
        # print(bpcfp_dict)
        sroted_fp_dict = dict(sorted(bpcfp_dict.items(), key=lambda x:x[0], reverse=False))
        # Draw result
        resToDraw = {}
        for root, dirs, files in os.walk(args.outdir):
            for file in fnmatch.filter(files, '*.json.ana.csv'):
                file_name = os.path.join(root, file)
                slice_name = file.replace('.json.ana.csv','')
                benchmark = slice_name.split('-')[0]
                sliceID = int(slice_name.split('-')[-1])
                resToDraw[slice_name] = []
                resToDraw[slice_name].append(getCoverSize(file_name)) 
                resToDraw[slice_name].append(blkNums[benchmark][sliceID])
        res = dict(sorted(resToDraw.items(), key=lambda x:x[0], reverse=False))
        drawSum(res, args.outdir)
        drawFP(sroted_fp_dict, args.outdir)
    else:
        print("Error input directory")


