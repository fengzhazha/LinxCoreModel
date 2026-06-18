"""
UR Value Count
"""
import os
import csv
import argparse
import sys

argp = argparse.ArgumentParser(description="Generate block_id & mask csv")
argp.add_argument("-i", "--input_file", default=".", type = str)
args = argp.parse_args()

class GetValueStatistic:
    def __init__(self, file_path):
        self.abi_map = {"ra": 0, "sp": 1, "a0": 2, "a1": 3, "a2": 4, "s0": 5, "fp": 5, 
                        "s1": 6, "s2": 7, "s3": 8, "s4": 9, "s5": 10,
                        "a3": 11, "a4": 12, "a5": 13, "a6": 14, "a7": 15}
        self.gpr_value_dict = {}
        self.file_path = file_path
        self.files_list = {}

    def parse_row(self, start_index, end_index, row_list):
        reg_list = []

        for index in range(start_index, end_index):
            if row_list[index] in self.abi_map:
                reg_list.append(self.abi_map[row_list[index]])

        return reg_list

    def parse_file(self):
        for row in sys.stdin:
            row_list = row.split(" ")
            if "B" in row_list[0] and row_list[1] == "BPC":
                if "GET" in row_list:
                    start_index = row_list.index("(") + 1
                    end_index = row_list.index(")")
                    get_list = self.parse_row(start_index, end_index, row_list)
                else:
                    get_list = []
            if ("R0:" in row_list[0]) and (len(get_list) > 0):
                for index, value in enumerate(row_list[:-1]):
                    if index in get_list:
                        gpr_value = value.split(":")[-1]
                        if gpr_value not in self.gpr_value_dict:
                            self.gpr_value_dict[gpr_value] = 1
                        else:
                            self.gpr_value_dict[gpr_value] += 1

    def generate_value_csv(self):
        csv_name = self.file_path + "_ur_value_count.csv"
        with open(csv_name, 'w', encoding='UTF8') as file:
            writer = csv.writer(file)
            first_row = ["gpr_value", "count"]
            writer.writerow(first_row)
            for gpr_value, count in self.gpr_value_dict.items():
                writer.writerow([gpr_value, count])

def main():
    print(args.input_file)
    instance = GetValueStatistic(args.input_file)
    instance.parse_file()
    instance.generate_value_csv()

if __name__ == '__main__':
    main()