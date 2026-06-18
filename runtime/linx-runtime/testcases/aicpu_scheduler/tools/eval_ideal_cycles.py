#!/usr/bin/python

import sys

def loadArraysFromFile():
    arrays = []
    with open(sys.argv[1], "r") as file:
        lines = file.readlines()
        for i in range(0, len(lines), 2):
            size = int(lines[i])
            array = list(map(int, lines[i+1].split()))
            arrays.append(array)
    return arrays

arrays = loadArraysFromFile()

_node = arrays[0]
_offset = arrays[1]
_pred = arrays[2]
_free_list = arrays[3]

print("len(_node) = %d" % (len(_node)))
print("len(_offset) = %d" % (len(_offset)))
print("len(_pred) = %d" % (len(_pred)))
print("len(_free_list) = %d" % (len(_free_list)))

aicore_num = int(sys.argv[2])

total_node = len(_node) - 2
finished_node = 0

iteration = 0

while finished_node < total_node:
    # print("finished_node = ", finished_node)
    ready_core_num = aicore_num
    resolve_list = []
    while len(_free_list) > 0 and ready_core_num > 0:
        resolve_list.append(_free_list.pop(0))
        ready_core_num -= 1
        finished_node += 1
    print("iter %d: %d node issued" % (iteration, aicore_num - ready_core_num))
    print(resolve_list)
    for n in resolve_list:
        for c in range(_node[n], _node[n+1]):
            _pred[_offset[c]] -= 1
            if _pred[_offset[c]] == 0:
                _free_list.append(_offset[c])
    iteration += 1

print("Total iteration: %d, Time cost: %d" % (iteration, iteration * 7500))
