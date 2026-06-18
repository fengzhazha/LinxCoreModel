#!/usr/bin/python3

import os
import sys
import argparse
import datetime
import time
import fnmatch
import subprocess
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

def getSliceFirst(sliceName):
    temp = sliceName.split('-')[0]
    return temp

def printCSV(input:List[List[int]], path:str):
    with open(path,'w') as csvfile:
        csvfile.write('overall coverage,length,repeat,sequence\n')
        for line in input:
            csvline:str = str(line[1]*line[2]) + ',' + str(line[1]) + ',' + str(line[2])
            for val in line[3:]:
                csvline += ',' + str(hex(val))
            csvfile.write(csvline+'\n')

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
    ckpt_id = {}
    ckpt_weight = {}
    max_weight_slice = {}
    outDir = {}
    # pool = Pool(processes=20)
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
            
            for file in fnmatch.filter(files, 'simpoints.txt'):
                file_name = os.path.join(root, file)
                current_dir = os.path.dirname(file_name)
                folder_name = os.path.basename(current_dir)
                ckpt_id[folder_name] = {}
                with open(file_name, 'r', encoding='utf-8') as f1:
                    for line in f1.readlines():
                        ckpt_id[folder_name][int(line.split()[1])] = int(line.split()[0])
            
            for file in fnmatch.filter(files, 'simpointWeights.txt'):
                file_name = os.path.join(root, file)
                current_dir = os.path.dirname(file_name)
                folder_name = os.path.basename(current_dir)
                ckpt_weight[folder_name] = {}
                with open(file_name, 'r', encoding='utf-8') as f1:
                    for line in f1.readlines():
                        ckpt_weight[folder_name][int(line.split()[1])] = float(line.split()[0])
        for key, value in ckpt_weight.items():
            max_id, max_weight = max(value.items(), key=lambda x: x[1])
            max_weight_slice[key] = ckpt_id[key][max_id]
        for root, dirs, files in os.walk(args.inputdir):
            # Run Function Model And Get Analysis data
            for file in fnmatch.filter(files, '*.json'):
                
                benchname = getSliceFirst(file)
                sliceid = getSliceId(file)
                if sliceid != max_weight_slice[benchname]:
                    continue
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


