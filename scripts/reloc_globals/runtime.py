import json
import os

""" Runtime Information for host global symbol and function """


class RuntimeInfo(object):
    def __init__(self):
        self.infos = list()

    def add_file_info(self, file_info):
        self.infos.append(file_info)

    # File names passing from runtime are exactly matched, but from object file is not complete.
    # For example, there are two files named as file.c, one is in dir1, and another is in dir2.
    # From runtime json, the file names are different: dir1/file.c and dir2/file.c
    # From object file ,the file name is read from symbol table and path info is not contained, the file names may conflict.
    def get_file_info(self, file_name, full_match=True):
        if full_match is True:
            for fi in self.infos:
                if fi.filename == file_name:
                    return fi

            return None
        else:
            info_list = list()
            for fi in self.infos:
                if os.path.split(fi.filename)[1] == os.path.split(file_name)[1]:
                    info_list.append(fi)
            if len(info_list) > 1:
                assert False, "multiple runtime info of file %s." % file_name
            elif len(info_list) == 1:
                return info_list[0]
            else:
                return None

    def get_symbol_addr(self, symbol, file):
        file_info = self.get_file_info(file, False)
        if file_info:
            return file_info.get_global(symbol)
        else:
            return None

    def get_symbol_info(self, symbol):
        for fi in self.infos:
            if fi.get_function(symbol) or fi.get_global(symbol):
                return symbol
        return None


class FileSymbolInfo(object):
    def __init__(self, file_name):
        self.filename = file_name
        self.functions = set()
        self.globals = dict()

    def add_function(self, func):
        self.functions.add(func)

    def add_global(self, global_name, global_addr):
        val = self.globals.get(global_name, None)
        if val is not None:
            assert (
                val == global_addr
            ), "find different address of global symbol %s in file %s" % (
                global_name,
                self.filename,
            )
        else:
            self.globals[global_name] = global_addr

    def get_function(self, func):
        for f in self.functions:
            if f == func:
                return f
        return None

    def get_global(self, global_name):
        if global_name in self.globals:
            return self.globals[global_name]
        else:
            return None


def parse_func_json(func_json, rt_info):
    func_name = func_json["func_name"]
    file_name = func_json["file_name"]
    file_rt = rt_info.get_file_info(file_name)
    if file_rt is None:
        file_rt = FileSymbolInfo(file_name)
        rt_info.add_file_info(file_rt)

    file_rt.add_function(func_name)
    for global_sym in func_json["globals"]:
        file_rt.add_global(global_sym["global_name"], global_sym["global_addr"])


def parse(json_file):
    with open(json_file) as jf:
        json_info = json.load(jf)
        rt_info = RuntimeInfo()

        for func_json in json_info:
            parse_func_json(func_json, rt_info)
        return rt_info
