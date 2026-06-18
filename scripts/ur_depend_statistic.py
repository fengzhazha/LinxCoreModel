"""
UR Dependency
"""

import os
import csv
import argparse
import sys

argp = argparse.ArgumentParser(description="Generate depend count & depend distance csv")
argp.add_argument("-n", "--block_num", default=-1, type = int)
argp.add_argument("-i", "--input_file", default=".", type = str)
args = argp.parse_args()

class UrValueSetCount:
    def __init__(self, file_path):
        self.file_path = file_path
        self.abi_map = {"ra": 0, "sp": 1, "a0": 2, "a1": 3, "a2": 4, "s0": 5, "fp": 5, 
                            "s1": 6, "s2": 7, "s3": 8, "s4": 9, "s5": 10,
                            "a3": 11, "a4": 12, "a5": 13, "a6": 14, "a7": 15}
        self.set_dict = {}
        self.count_statistic = {}
        self.matrix = []
        self.set_block_id = {}
        self.distance_statistic = {}
        self.distance_dict = {}

    def parse_row(self, start_index, end_index, row_list):
        reg_list = []

        for index in range(start_index, end_index):
            if row_list[index] in self.abi_map:
                reg_list.append(self.abi_map[row_list[index]])

        return reg_list

    def parse_file(self):
        column_num = 17
        
        first_row = ["BPC"]

        for i in range(column_num-1):
            first_row.append("R" + str(i))

        self.matrix.append(first_row)
        
        for row in sys.stdin:
            row_list = row.split(" ")
            row_result = ["" for i in range(column_num)]
            set_list = []
            get_list = []
            if "B" in row_list[0] and row_list[1] == "BPC":
                block_num_str = row_list[0][1:]
                if (args.block_num != -1) and (int(block_num_str) >= args.block_num):
                    break
                # bpc
                row_result[0] = row_list[2]
                if "GET" in row_list and "SET" not in row_list:
                    start_index = row_list.index("(") + 1
                    end_index = row_list.index(")")
                    get_list = self.parse_row(start_index, end_index, row_list)
                    distance = self.get_update(get_list, int(block_num_str) + 1)

                if "SET" in row_list and "GET" not in row_list:
                    start_index = row_list.index("(") + 1
                    end_index = row_list.index(")")
                    set_list = self.parse_row(start_index, end_index, row_list)
                    self.set_update(set_list, int(block_num_str) + 1)

                if "GET" in row_list and "SET" in row_list:
                    get_start_index = row_list.index("(") + 1
                    get_end_index = row_list.index(")")
                    get_list = self.parse_row(get_start_index, get_end_index, row_list)
                    set_start_index = row_list.index("(", get_end_index + 1)
                    set_end_index = row_list.index(")", get_end_index + 1)
                    set_list = self.parse_row(set_start_index, set_end_index, row_list)
                    distance = self.get_update(get_list, int(block_num_str) + 1)
                    self.set_update(set_list, int(block_num_str) + 1)

                for get_index, value in enumerate(get_list):
                    if value in self.distance_dict:
                        row_result[value + 1] = str(self.distance_dict[value]) + " get"
                    else:
                        row_result[value + 1] = "get"
                
                self.distance_dict.clear()

                for set_index, value in enumerate(set_list):
                    if "get" in row_result[value + 1]:
                        row_result[value + 1] = row_result[value + 1] + ".set"
                    else:
                        row_result[value + 1] = "set"

                self.matrix.append(row_result)
            if "R0:" in row_list[0]:
                for i in range(column_num):
                    if i > 0:
                        self.matrix[-1][i] = row_list[i-1].split(":")[-1] + " " + self.matrix[-1][i]
        # last set.g statistic 
        self.final_update()

    def final_update(self):
        for reg_id, info in self.set_dict.items():
            # last set row index
            row_index = info[0]
            # last set column index
            column_index = info[1]
            count = info[2]
            if count not in self.count_statistic:
                self.count_statistic[count] = 1
            else:
                self.count_statistic[count] += 1
            self.matrix[row_index][column_index] = self.matrix[row_index][column_index] + " " + str(count)

    def set_update(self, set_list, row):
        for index, reg_id in enumerate(set_list):
            # use count
            if reg_id not in self.set_dict:
                # key: reg_id, value: row_num, column_num, times 
                self.set_dict[reg_id] = [row, reg_id + 1, 0]
            else:
                # previous set row index
                row_index = self.set_dict[reg_id][0]
                # previous set column index
                column_index = self.set_dict[reg_id][1]
                count = self.set_dict[reg_id][2]
                if "set" in self.matrix[row_index][column_index]:
                    if count not in self.count_statistic:
                        self.count_statistic[count] = 1
                    else:
                        self.count_statistic[count] += 1
                    self.matrix[row_index][column_index] = self.matrix[row_index][column_index] + " " + str(count)
                    self.set_dict[reg_id] = [row, reg_id + 1, 0]
                else:
                    print("error: current position hasn't set")
            # distance count
            self.set_block_id[reg_id] = row

    def get_update(self, get_list, row):
        for index, reg_id in enumerate(get_list):
            if reg_id in self.set_dict:
                self.set_dict[reg_id][2] += 1
            if reg_id in self.set_block_id:
                distance = row - self.set_block_id[reg_id]
                self.distance_dict[reg_id] = distance
                if distance not in self.distance_statistic:
                    self.distance_statistic[distance] = 1
                else:
                    self.distance_statistic[distance] += 1
                
        
    def generate_mask_csv(self):
        csv_name = self.file_path + "_bpc_gpr_gs.csv"
        with open(csv_name, 'w', encoding='UTF8') as file:
            writer = csv.writer(file)
            for index, row in enumerate(self.matrix):
                writer.writerow(row)

    def generate_count_csv(self):
        csv_statistic_name = self.file_path + "_ur_depend_count.csv"
        with open(csv_statistic_name, 'w', encoding='UTF8') as file:
            writer = csv.writer(file)
            first_row = ["get_num_after_set", "num", "rate"]
            writer.writerow(first_row)
            num_sum = sum(list(self.count_statistic.values()))
            for get_num_after_set, value in self.count_statistic.items():
                row = [get_num_after_set, value, value / num_sum]
                writer.writerow(row)
    
    def generate_distance_csv(self):
        csv_distance_name = self.file_path + "_ur_depend_distance.csv"
        with open(csv_distance_name, 'w', encoding='UTF8') as file:
            writer = csv.writer(file)
            first_row = ["distance", "num", "rate"]
            writer.writerow(first_row)
            distance_sum = sum(list(self.distance_statistic.values()))
            for distance, distance_num in self.distance_statistic.items():
                row = [distance, distance_num, distance_num / distance_sum]
                writer.writerow(row)

def main():
    print(args.input_file)
    instance = UrValueSetCount(args.input_file)
    instance.parse_file()
    instance.generate_mask_csv()
    instance.generate_count_csv()
    instance.generate_distance_csv()

if __name__ == '__main__':
    main()
