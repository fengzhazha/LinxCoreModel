
import sys
import re

allStr = ''

FUNCTION_MAP = {
    'Extract32': 'Deposit32',
    'Sextract32': 'Deposit32',
    'Extract64': 'Deposit64',
    'Sextract64': 'Deposit64',
    'ExShift1': 'ExShift1Right',
    'ExShift2': 'ExShift2Right',
    'ExShift3': 'ExShift3Right',
    'ExShift12': 'ExShift12Right',
}

TYPE_MAP = {
    '16': 'uint16_t',
    '32': 'uint32_t',
    '64': 'uint64_t',
}

# 有4+1种语句需要匹配:
# inst->src2 = MInst::Extract32(bin, 2, 5);
# inst->src2 = MInst::ExShift1(MInst::Sextract32(bin, 25, 7));
# inst->src2 = MInst::Deposit(MInst::Extract64(bin, 59, 5), 5, MInst::Extract64(bin, 52, 5));
# inst->src2 = MInst::ExShift12(MInst::Deposit(MInst::Extract64(bin, 44, 8), 8, MInst::Sextract64(bin, 52, 12)));
# static InstInfo* DecodeBlock32_extract_arith_si(InstInfo *inst, uint32_t bin)
EXTRACT_PATTERN = re.compile(r'(\s*)(inst->\w+\d*)\s*=\s*MInst::((?:E|Se)xtract\d+)\((bin,\s*\d+,\s*\d+)\);')
EXSHFIT_EXTRACT_PATTERN = re.compile(r'(\s*)(inst->\w+\d*)\s*=\s*MInst::(ExShift\d+)\(MInst::((?:E|Se)xtract\d+)\((bin,\s*\d+,\s*\d+)\)\);')
DEPOSIT_EXTRACT_PATTERN = re.compile(r'(\s*)(inst->\w+\d*)\s*=\s*MInst::Deposit\(MInst::((?:E|Se)xtract\d+)\((bin,\s*\d+,\s*\d+)\),\s*(\d+),\s*MInst::((?:E|Se)xtract\d+)\((bin,\s*\d+,\s*\d+)\)\);')
EXSHIFT_DEPOSIT_EXTRACT_PATTERN = re.compile(r'(\s*)(inst->\w+\d*)\s*=\s*MInst::(ExShift\d+)\(MInst::Deposit\(MInst::((?:E|Se)xtract\d+)\((bin,\s*\d+,\s*\d+)\),\s*(\d+),\s*MInst::((?:E|Se)xtract\d+)\((bin,\s*\d+,\s*\d+)\)\)\);')

DECODE_PATTERN = re.compile(r'DecodeInst\d+_extract_')

def parse_binary_string(s):
    binary = []
    for c in s:
        if c in (' ', '\t'):
            continue
        elif c in ('-', '.', '0'):
            binary.append('0')
        elif c == '1':
            binary.append('1')
        else:
            break
    return ''.join(binary)

def GenFixedField(input_path, size):
    global allStr
    type = TYPE_MAP.get(size, None)
    try:
        with open(input_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        allStr += "// Generated from input file using Python\n"
        allStr += "#include \"../../ISACommon/InstInfo.h\"\n"
        allStr += "#include <unordered_map>\n"
        allStr += "\n"
        allStr += "namespace JCore {\n"
        allStr += "std::unordered_map<Opcode, " + type + "> op2bin_" + size + " = {\n"

        for line_num, line in enumerate(lines, start=1):
            line = re.sub(r'^\s{0,4}', '', line) # 去除0-4个空格
            if not line:
                continue

            match = re.match(r'^([a-zA-Z][^\s]*)', line)
            if not match:
                continue
            key = match.group(1)
            rest = line[match.end():]
            binary_str = parse_binary_string(rest)
            if not binary_str:
                allStr += f"// No valid binary found for key: '{key}'\n"
                continue

            length = len(binary_str)
            if length not in {16, 32, 64}:
                allStr += f"// Warning: Binary length {length} for key '{key}' is invalid.\n"
                continue

            if key[0].islower():
                cpp_key = f"op_{key}"
            else:
                cpp_key = f"Opcode::OP_{key}"
            allStr += f"    {{{cpp_key}, 0b{binary_str}}},\n"
        allStr += "};\n"
        # print(f"Suaccelssfully generated C++ code 1/3'")
    except FileNotFoundError:
        print(f"Error: File '{input_path}' not found.")
    except Exception as e:
        print(f"An error occurred: {e}")

def ExtraVariableField(input_path, size):
    global allStr
    try:
        with open(input_path, 'r', encoding='utf-8') as f_in:
            a_list = []
            found_start = False
            for line in f_in:
                if not found_start:
                    # 第一阶段：输出包含 static/xtract/{/} 的行
                    if any(keyword in line for keyword in ['static', 'xtract', '{', '}']) and 'namespace' not in line:
                        allStr += line
                    # 检查是否遇到起始函数
                    if any(func in line for func in [
                        'InstInfo* DecodeInst16(',
                        'InstInfo* DecodeInst32(',
                        'InstInfo* DecodeInst48(',
                        'InstInfo* DecodeInst64('
                    ]):
                        found_start = True
                        type = TYPE_MAP.get(size, None)
                        if not type:
                            print(f"Warning: no {type} in TYPE_MAP")
                        allStr += (type + " EncodeInst" + size + "(InstInfo* inst){\n")
                        allStr += ("    " + type + " bin{0};\n")
                        allStr += ("    auto it = op2bin_" + size + ".find(inst->opcode);\n")
                        allStr += ("    if (it == op2bin_" + size + ".end()) {\n")
                        allStr += ('        std::cerr << \"Warning: inst->opcode is illegal: " << OpcodeManager::Inst().GetOpcodeStr(inst->opcode) << std::endl;\n')
                        allStr += ("        return 0ull;\n")
                        allStr += ("    }\n")
                        allStr += ("    bin = it->second;\n")
                        allStr += ("    switch(inst->opcode) {\n")
                    continue  # 跳过未找到起始函数的行

                # 第二阶段：处理匹配的行
                if not line:
                    continue
                if 'xtract' in line:
                    # 加入到 a_list 最后一个 list
                    if a_list:
                        a_list[-1].append(line.strip())
                elif 'case' in line or '{' in line:
                    # 新增一个空 list
                    a_list.append([])
                elif 'break' in line or '}' in line:
                    # 弹出最后一个 list
                    if a_list:
                        a_list.pop()
                elif 'inst->opcode' in line:
                    # 提取 OP
                    op_part = line.split('inst->opcode = ')[-1]
                    op = op_part.strip().rstrip(';').strip()
                    allStr += f'        case {op}:\n'

                    # 输出 a_list 中的每一行
                    for lst in a_list:
                        for item in lst:
                            allStr += (f'            {item}\n')
                    allStr += "            break;\n"
            allStr += "        default:\n"
            allStr += '            std::cerr << "Warning: Not match any encoding func of inst->opcode: " << OpcodeManager::Inst().GetOpcodeStr(inst->opcode) << std::endl;\n'
            allStr += "            break;\n"
            allStr += "    }\n"
            allStr += "    return bin;\n"
            allStr += "}\n\n"
        # print(f"Suaccelssfully generated C++ code 2/3'")
    except FileNotFoundError:
        print(f"Error: File '{input_path}' not found.")
    except Exception as e:
        print(f"An error occurred: {e}")

def ProcessExtract(pattern_match, line):
    tab = pattern_match.group(1)
    inst_src = pattern_match.group(2)
    extract = pattern_match.group(3)
    var = pattern_match.group(4)
    extract_replace = FUNCTION_MAP.get(extract, None)
    if not extract_replace:
        print(f"Warning: no {extract} in FUNCTION_MAP")
        return line
    new_line = f'{tab}bin = MInst::{extract_replace}({var}, {inst_src});'
    return f'{tab}// {line.strip()}\n{new_line}'

def ProcessExshiftExtract(pattern_match, line):
    tab = pattern_match.group(1)
    inst_src = pattern_match.group(2)
    exshift = pattern_match.group(3)
    extract = pattern_match.group(4)
    var = pattern_match.group(5)
    exshift_replace = FUNCTION_MAP.get(exshift, None)
    extract_replace = FUNCTION_MAP.get(extract, None)
    if not exshift_replace:
        print(f"Warning: no {exshift_replace} in FUNCTION_MAP")
        return line
    if not extract_replace:
        print(f"Warning: no {extract_replace} in FUNCTION_MAP")
        return line
    new_line = f'{tab}{{\n'
    new_line += f'{tab}auto tmp = MInst::{exshift_replace}({inst_src});\n'
    new_line += f'{tab}bin = MInst::{extract_replace}({var}, tmp);\n'
    new_line += f'{tab}}}'
    return f'{tab}// {line.strip()}\n{new_line}'

def ProcessDepositExtract(pattern_match, line):
    tab = pattern_match.group(1)
    inst_src = pattern_match.group(2)
    extract1 = pattern_match.group(3)
    var1 = pattern_match.group(4)
    pos = pattern_match.group(5)
    extract2 = pattern_match.group(6)
    var2 = pattern_match.group(7)
    extract1_replace = FUNCTION_MAP.get(extract1, None)
    extract2_replace = FUNCTION_MAP.get(extract2, None)
    if not extract1_replace:
        print(f"Warning: no {extract1_replace} in FUNCTION_MAP")
        return line
    if not extract2_replace:
        print(f"Warning: no {extract2_replace} in FUNCTION_MAP")
        return line
    new_line = f'{tab}bin = MInst::{extract1_replace}({var1}, {inst_src});\n'
    new_line += f'{tab}bin = MInst::{extract2_replace}({var2}, {inst_src} >> {pos});'
    return f'{tab}// {line.strip()}\n{new_line}'

def ProcessExshiftDepositExtract(pattern_match, line):
    tab = pattern_match.group(1)
    inst_src = pattern_match.group(2)
    exshift = pattern_match.group(3)
    extract1 = pattern_match.group(4)
    var1 = pattern_match.group(5)
    pos = pattern_match.group(6)
    extract2 = pattern_match.group(7)
    var2 = pattern_match.group(8)
    exshift_replace = FUNCTION_MAP.get(exshift, None)
    extract1_replace = FUNCTION_MAP.get(extract1, None)
    extract2_replace = FUNCTION_MAP.get(extract2, None)
    if not exshift_replace:
        print(f"Warning: no {exshift_replace} in FUNCTION_MAP")
        return line
    if not extract1_replace:
        print(f"Warning: no {extract1_replace} in FUNCTION_MAP")
        return line
    if not extract2_replace:
        print(f"Warning: no {extract2_replace} in FUNCTION_MAP")
        return line
    new_line = f'{tab}{{\n'
    new_line += f'{tab}auto tmp = MInst::{exshift_replace}({inst_src});\n'
    new_line += f'{tab}bin = MInst::{extract1_replace}({var1}, tmp);\n'
    new_line += f'{tab}bin = MInst::{extract2_replace}({var2}, tmp >> {pos});\n'
    new_line += f'{tab}}}'
    return f'{tab}// {line.strip()}\n{new_line}'

def MatchLine(line):
    match_rules = {
        EXTRACT_PATTERN : ProcessExtract,
        EXSHFIT_EXTRACT_PATTERN : ProcessExshiftExtract,
        DEPOSIT_EXTRACT_PATTERN : ProcessDepositExtract,
        EXSHIFT_DEPOSIT_EXTRACT_PATTERN : ProcessExshiftDepositExtract,
    }

    for pattern, func in match_rules.items():
        pattern_match = pattern.match(line)
        if pattern_match:
            return func(pattern_match, line)

    if DECODE_PATTERN.search(line):
        new_line = re.sub(r'DecodeInst(\d+)_extract_', r'EncodeInst\g<1>_extract_', line)
        new_line = re.sub(r'static InstInfo\*', r'static void', new_line)
        new_line = re.sub(r'_t bin\)', r'_t &bin)', new_line) # uint16/32/64_t bin)
        return new_line

    return line

def DepositVariableField(output_path):
    global allStr
    lines = allStr.splitlines()
    lines_replace = []
    for line in lines:
        lines_replace.append(MatchLine(line))
    allStr = '\n'.join(lines_replace)
    allStr += "} // namespace JCore"
    # print(f"Suaccelssfully generated C++ code 3/3'")

def WriteFile(output_path):
    global allStr
    with open(output_path, 'w', encoding='utf-8') as out_f:
        out_f.write(allStr)

def main():
    if len(sys.argv) != 5:
        print("Usage: python xxx.py block16.decode decode-inst16.cpp encode-inst16.cpp 16")
        return
    fileFixed = sys.argv[1]
    fileVar = sys.argv[2]
    fileOutput = sys.argv[3]
    size = sys.argv[4]
    GenFixedField(fileFixed, size)
    ExtraVariableField(fileVar, size)
    DepositVariableField(fileOutput)
    WriteFile(fileOutput)

if __name__ == "__main__":
    main()
