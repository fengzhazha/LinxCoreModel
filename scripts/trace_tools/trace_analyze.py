import  sys, re

header_infos = {}
minst_infos = {}
func_infos = {}

def parse_header_or_minst(pc, minst, line):
    match = re.search('ptr:0x([a-f0-9]+)\ssize:0x([a-f0-9]+)', line)
    if match:
        # header string
        tpc = match.group(1)
        size = int(match.group(2), 16)
        header_infos[pc] = (tpc, size)
    else:
        # minst string
        minst_infos[pc] = minst

def parse_objdump(file_name):
    with open(file_name, 'r') as f:
        for line in f:
            match1 = re.search('^\s+([a-f0-9]+):(.+)$', line)
            if match1:
                parse_header_or_minst(match1.group(1), match1.group(2), line)


def parse_symtable(file_name):
    with open(file_name, 'r') as f:
        for line in f:
            match1 = re.search('^\s*[0-9]+:\s([a-f0-9]+)\s+([0-9]+)\sFUNC', line)
            if not match1:
                continue
            entry_bpc = match1.group(1)
            size = int(match1.group(2))

            match2 = re.search('FUNC\s+[A-Z]+\s+[A-Z]+\s+[0-9]+\s(.*?)$', line)
            if not match2:
                assert 0

            func_name = match2.group(1)

            for i in range(int(size/8)):
                bpc = int(entry_bpc, 16) + i*8
                func_infos[bpc] = func_name


def parse_bpc_trace(file_name, dump_minst):
    with open(file_name, 'r') as f:
        for line in f:
            match1 = re.search('([a-f0-9]+)', line)
            if not match1:
                assert 0
            bpc = match1.group(1)

            if dump_minst:
                start_tpc = header_infos[bpc][0]
                size = header_infos[bpc][1]

                for i in range(size):
                    tpc = hex(int(start_tpc, 16) + i * 2)[2:]
                    print("{}: {}".format(tpc, minst_infos[tpc]))
            else:
                func_name = func_infos[int(bpc, 16)] if int(bpc, 16) in func_infos else "Not found, check objdump file"
                print("{}: {}".format(bpc, func_name))


def main():
    if (sys.argv[1] == '1'):
        parse_objdump(sys.argv[2])
        parse_bpc_trace(sys.argv[3], True)
    elif(sys.argv[1] == '2'):
        parse_symtable(sys.argv[2])
        parse_bpc_trace(sys.argv[3], False)
    else:
        print("only support mode 1 and mode 2")


if __name__ == "__main__":
   #parse_objdump(sys.argv[1])
   main()

   #for bpc in header_infos:
   #     print('{} {} {}'.format(bpc, header_infos[bpc][0], header_infos[bpc][1]))
   #for tpc in minst_infos:
   #     print ("{} {}".format(tpc, minst_infos[tpc]))
