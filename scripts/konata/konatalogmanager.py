import re
import sys
import os
import shutil

PARAMETER_ERR_CODE = -1
RET_OK = 0


class KonataLogManager:
    def __init__(self):
        self.mode = None
        self.src_path = None
        self.des_path = None
        self.current_cycle = 0
        self.max_life_cycle = 5000  # maximum number of instruction lifecycles
        self.max_idle_cycle = 5000  # the maximum idle time of the model
        self.check_inst_life_cycle = 1000  # Instruction lifecycle check frequency

    def init(self, argv):
        arg_cnt = len(argv)
        if arg_cnt < 3:
            print("parameter error！")
            return PARAMETER_ERR_CODE

        self.mode = argv[1]
        self.src_path = argv[2]
        if not os.path.exists(self.src_path):
            print("The file does not exist. check the file path!")
            return PARAMETER_ERR_CODE
        if arg_cnt == 4:
            self.des_path = argv[3]
        else:
            pardir = os.path.abspath(os.path.join(self.src_path, os.pardir))
            self.des_path = os.path.join(pardir, "konata_ouput.txt")

        return RET_OK

    def get_parent_dir(self, path):
        return os.path.abspath(os.path.join(path, os.pardir))

    def get_api_type(self, line):
        if line[0] == 'C' and line[1] == '=':
            return 'T'
        return line[0]

    def get_inst_info(self, line):
        return re.split(r"\s+", line)

    def get_file_id(self, line):
        if self.get_api_type(line) == 'C':
            return '-1'
        else:
            return self.get_inst_info(line)[1]

    def get_inst_stage(self, line):
        return self.get_inst_info(line)[3]

    def get_inst_thread_num(self, line):
        return int(self.get_inst_info(line)[3])

    def is_inst_flush(self, line):
        return self.get_inst_info(line)[3] == '1'

    def check_inst(self, inst_map, writer, auto_retire_set):
        if self.current_cycle % self.check_inst_life_cycle == 0:
            for inst_file_id in list(inst_map.keys()):
                start_cycle = inst_map[inst_file_id]
                if self.current_cycle - start_cycle > self.max_life_cycle:
                    print("file id {}\tAUTO_RETIRE\n".format(inst_file_id))
                    writer.write("L\t{}\t0\tAUTO_RETIRE\n".format(inst_file_id))
                    writer.write("R\t{}\t{}\t0\n".format(inst_file_id, inst_file_id))
                    auto_retire_set.add(inst_file_id)
                    del inst_map[inst_file_id]

    # Automatically end instruction lifecycle
    # parameter 1
    def auto_retire(self):
        inst_map = {}
        total_idle_cycle = 0
        auto_retire_set = set()
        with open(self.src_path, 'r') as reader, open(self.des_path, 'w') as writer:
            for line in reader:
                file_id = self.get_file_id(line)
                if file_id in auto_retire_set:
                    continue
                writer.write(line)
                if self.get_api_type(line) == 'C':
                    self.current_cycle += 1
                    total_idle_cycle += 1
                    self.check_inst(inst_map, writer, auto_retire_set)
                elif self.get_api_type(line) == 'I':
                    total_idle_cycle = 0
                    if file_id in inst_map:
                        print("ERROR:file id {} already exists!".format(file_id))
                    else:
                        inst_map[file_id] = self.current_cycle
                elif self.get_api_type(line) == 'S':
                    total_idle_cycle = 0
                    if file_id not in inst_map:
                        if self.get_inst_stage(line) != 'W2':
                            print("ERROR:file id {} does not exists!".format(file_id))
                elif self.get_api_type(line) == 'R':
                    total_idle_cycle = 0
                    if file_id in inst_map:
                        inst_map.pop(file_id)
                    else:
                        if not self.is_inst_flush(line):
                            print("ERROR_ID:file id {} already retire!".format(file_id))

                if total_idle_cycle > self.max_idle_cycle:
                    for inst_file_id in inst_map.keys():
                        print("file id {}\tAUTO_RETIRE\n".format(inst_file_id))
                        writer.write("L\t{}\t0\tAUTO_RETIRE\n".format(inst_file_id))
                        writer.write("R\t{}\t{}\t0\n".format(inst_file_id, inst_file_id))
                    break

        print("The maximum lifecycle of the instruction is {} cycle.".format(self.max_life_cycle))
        print("The maximum idle time of the model is {} cycle.".format(self.max_life_cycle))
        print("output log path:{}.".format(os.path.abspath(self.des_path)))
        print("finish!")

    def get_thead_num(self):
        thread_set = set()
        max_count = 10000
        cur_count = 0
        with open(self.src_path, 'r') as reader:
            for line in reader:
                if self.get_api_type(line) == 'I':
                    thread_set.add(self.get_inst_thread_num(line))
                    cur_count += 1
                    if cur_count > max_count:
                        break
        return len(thread_set)

    def write_to_all(self, writer_list, line):
        for i in range(len(writer_list)):
            writer_list[i].write(line)

    def replace_fid_to_rid(self, type, line, rid):
        line_array = line.split("\t")
        line_array[1] = str(rid)
        if type == 'R':
            line_array[2] = str(rid)
        separator = '\t'
        return separator.join(line_array)

    def fix_retireid(self, fileid_retireid_map, des_path_list):
        for des_path in des_path_list:
            fix_des_path = des_path.split(".")[0] + "_fix" + "." + self.des_path.split(".")[1]
            share_set = {'C', 'K'}
            with open(des_path, 'r') as reader, open(fix_des_path, 'w') as writer:
                for line in reader:
                    type = line[0]
                    if type in share_set:
                        writer.write(line)
                    else:
                        fid = self.get_file_id(line)
                        if fid in fileid_retireid_map:
                            rid = fileid_retireid_map[fid]
                            writer.write(self.replace_fid_to_rid(type, line, rid))
            os.remove(des_path)
            shutil.move(fix_des_path, des_path)
            print("fix {} done!".format(des_path))

    def split_by_thread(self):
        print("thread num:{}".format(self.get_thead_num()))
        thread_num = self.get_thead_num()
        writer_list = []
        share_set = {'C', 'K'}
        private_set = {'I', 'S', 'E', 'R', 'L'}
        fileid_thread_map = {}
        fileid_retireid_map = {}
        cur_retire_id_list = [0] * thread_num
        des_path_list = []
        for i in range(thread_num):
            des_path = self.des_path.split(".")[0] + "_" + str(i) + "." + self.des_path.split(".")[1]
            des_path_list.append(des_path)
            print("output path {}".format(des_path))
            writer = open(des_path, "w")
            writer_list.append(writer)
        with open(self.src_path, 'r') as reader:
            for line in reader:
                type = line[0]
                if type in share_set:
                    self.write_to_all(writer_list, line)
                elif type in private_set:
                    fid = self.get_file_id(line)
                    if type == 'I':
                        tid = self.get_inst_thread_num(line)
                        fileid_thread_map[fid] = tid
                        writer_list[tid].write(line)
                    elif type == 'R':
                        tid = fileid_thread_map[fid]
                        fileid_retireid_map[fid] = cur_retire_id_list[tid]
                        cur_retire_id_list[tid] += 1
                        writer_list[tid].write(line)
                    else:
                        tid = fileid_thread_map[fid]
                        writer_list[tid].write(line)
        for i in range(thread_num):
            writer_list[i].close()
        print("start fix rid:")
        self.fix_retireid(fileid_retireid_map, des_path_list)
        print("SUACCELSS")

    def get_thread_num(self):
        max_line_index = 1000
        cur_line_index = 0
        thread_num_set = set()
        with open(self.src_path, "r") as reader:
            for line in reader:
                if self.get_api_type(line) == 'C':
                    continue
                elif self.get_api_type(line) == 'I':
                    thread_num_set.add(self.get_inst_thread_num(line))
                cur_line_index += 1
                if cur_line_index > max_line_index:
                    break

        return len(thread_num_set)

    def write_log_to_writer(self, writer_list, log):
        for writer in writer_list:
            writer.write(log)

    def process_log(self):
        if self.mode == '1':
            self.auto_retire()
        elif self.mode == '2':
            self.split_by_thread()


if __name__ == '__main__':
    konataManager = KonataLogManager()
    ret = konataManager.init(sys.argv)
    if ret != RET_OK:
        print(ret)
        sys.exit(ret)
    konataManager.process_log()
