"""
Generate Pe Load Trace
"""

import json
import argparse
import sys

argParser = argparse.ArgumentParser(description="Generate pe load trace")
argParser.add_argument("-i", "--input_file", metavar="ckpt dir", type=str, help="ckpt dir")
args = argParser.parse_args()

class PeLoadTrace:
    def __init__(self, file_path):
        self.file_path = file_path
        self.pe_id = 0
        self.block_name = ""
        self.json_list = []
        self.pe_num = 5
        self.pe_row_num = 64
        self.cur_pe_row_last_retire_cycle = [[-1 for j in range(self.pe_row_num)] for i in range(self.pe_num)]
        self.pe_block_num = [0, 0, 0, 0, 0]
        self.collect_inst_info = False
        self.inst_cycle = []
        self.decodeCycle = []
     
    def write_pe_period(self):
        if self.inst_cycle:
            cur_rob_enter_cycle = min(self.decodeCycle)
            cur_rob_retire_cycle = max(self.inst_cycle)

            if cur_rob_enter_cycle > 0 and cur_rob_retire_cycle > 0 and cur_rob_enter_cycle < cur_rob_retire_cycle:   
        
                # part block info
                block_info_part = {}
                for row_index in range(self.pe_row_num):
                    if cur_rob_enter_cycle >= self.cur_pe_row_last_retire_cycle[self.pe_id][row_index]:
                        block_info_part = {"name": self.block_name, "ph": "X", "pid": self.pe_id, "tid": row_index, "ts": cur_rob_enter_cycle, 
                        "dur": cur_rob_retire_cycle - cur_rob_enter_cycle}
                        self.cur_pe_row_last_retire_cycle[self.pe_id][row_index] = cur_rob_retire_cycle
                        break
                if not block_info_part:
                    tid = self.pe_row_num - 1 - self.pe_block_num[self.pe_id] % self.pe_row_num
                    block_info_part = {"name": self.block_name, "ph": "X", "pid": self.pe_id, "tid": tid,
                        "ts": cur_rob_enter_cycle, "dur": cur_rob_retire_cycle - cur_rob_enter_cycle}
                    self.cur_pe_row_last_retire_cycle[self.pe_id][tid] = cur_rob_retire_cycle
                    self.pe_block_num[self.pe_id] += 1
                
                self.json_list.append(block_info_part)

    def parse_file(self):
        sys.stdin.reconfigure(errors='ignore')
        align_block = {"name": "Align block", "ph": "X", "pid": 0, "tid": 0, "ts": 0, "dur": 1}
        self.json_list.append(align_block)
        for line in sys.stdin:
            # head info
            if "BROB head info" in line or "SoC_MP's heart beats" in line:
                continue
            elif "O3PipeView:f0:" in line and "peid" in line:
                self.write_pe_period()
                self.inst_cycle.clear()
                self.decodeCycle.clear()
                self.collect_inst_info = False
                self.pe_id = int(line.split(":")[-1].split(" ")[2])
                block_id_start = line.index("[B") + 2
                block_id_end = line.index("]")
                if len(line.split(":")[-1].split(" ")) == 6:
                    self.block_name = "B" + line[block_id_start:block_id_end] + " " + line.split(" ")[-1].strip()
                else:
                    bpc = line.split(":")[3]
                    self.block_name = "B" + line[block_id_start:block_id_end] + " " + bpc
            # inst info
            elif "O3PipeView:fetch" in line and ("OPE" in line or "MemPE" in line) and not self.collect_inst_info:
                self.collect_inst_info = True
            elif "BlockISA CPU Stats" in line:
                self.write_pe_period()
                break
            elif self.collect_inst_info:  
                cycle = int(line.split(":")[2].strip())
                if "decode:" in line:
                    self.decodeCycle.append(cycle)
                if ("fetch:" not in line) and ("fetchReturn:" not in line) and ("f0:" not in line) and ("f1:" not in line):
                    self.inst_cycle.append(cycle)
                
        json_str = json.dumps(self.json_list)

        with open(self.file_path+"_pe_load.json", 'w') as f:
            f.write(json_str)

def main():
    instance = PeLoadTrace(args.input_file)
    instance.parse_file()
    
if __name__ == '__main__' :
    main()

