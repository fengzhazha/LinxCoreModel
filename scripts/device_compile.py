import json
import os
import sys
import time
import argparse

""" Automated Heterogeneous Compilation Scripts. """

parse = argparse.ArgumentParser()
parse.add_argument("name")
args = parse.parse_args()


#The location where the intermediate .o files are stored.
o_path = "./output"

#path to the blockisa compiler.
compile_pre = "${compiler_path} "

compile_option = "--target=linx64 -march=linx64im -mabi=lp64 -fno-jump-tables -O2"
compile_ful = "\n"
compile_out = "-o "

#RELOC_BIN:path to the binary relocation scripts.
reloc_pre = "${RELOC_BIN} --runtime {json} --device ".format(RELOC_BIN="RELOC_BIN", json=args.name)
reloc_ful = "--debug"
link_pre = "${compiler_path} -O2 "
link_ful = "  -o bisa_kernel.o --target=linx64 -march=linx64im -mabi=lp64 -fno-jump-tables  -nostartfiles"

if not os.path.exists(o_path):
    os.mkdir(o_path)


with open(args.name, 'r') as fp, open('dev_compile.sh', 'w') as fc, \
open('dev_reloc.sh', 'w') as fr, open('dev_link.sh', 'w') as fl:
    fc.truncate(0)
    fr.truncate(0)
    fl.truncate(0)
    json_data = json.load(fp)
    num_ = len(json_data)
    exist_file = ""
    reloc_str = ""
    link_str = ""
    link_out = ""
    for i in range(num_):
        v = json_data[i]['file_name']
        if v not in exist_file:
            compile_str = ""
            compile_out = " -o "
            
            exist_file += v
            index_name = v.rfind("/")
            if index_name== -1:
                file_name = v
            else:
                file_name = v[index_name:] 
            compile_out += o_path + "/" +file_name + ".o"
            link_out += o_path + "/" + file_name + ".o" + " "
            compile_str = compile_pre + compile_option +  " -c " + v + compile_out + compile_ful
            fc.writelines(compile_str)
            reloc_str += o_path + "/" + file_name + ".o" + " "
    reloc_str = reloc_pre + reloc_str + reloc_ful
    link_str = link_pre + link_out + link_ful
    fr.writelines(reloc_str)
    fl.writelines(link_str)


time.sleep(5)
command = "bash dev_compile.sh"
res = os.system(command)
if res == 1:
    print("exe dev_compile.sh failed")
    sys.exit()
print("compile done")

time.sleep(5)
command = "bash dev_reloc.sh"
res = os.system(command)
if res == 1:
    print("exe dev_reloc.sh failed")
    sys.exit()
print("reloc done")


time.sleep(5)
command = "bash dev_link.sh"
res = os.system(command)
if res == 1:
    print("exe dev_link.sh failed")
    sys.exit()
print("link done")
