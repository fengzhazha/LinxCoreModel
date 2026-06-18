#include <unistd.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <stack>
#include <string>

#include "model_rt.h"
#include "json.hpp"

using ordered_json = nlohmann::ordered_json;
using namespace std;

class GlobalVariable {
private:
    string name;
    unsigned long addr;

public:
    GlobalVariable(string s, unsigned long v)
    {
    name = s;
    addr = v;
    }
    ~GlobalVariable(){};

    void __attribute__((noinline)) dump()
    {
        cerr << name << ": " << hex << addr << endl;
    }

    string getName() { return name; }
    unsigned long getAddress() { return addr; }
};

class RuntimeInfo {
private:
    string func_name;
    string file_name;
    unsigned long exec_times;
    vector<GlobalVariable *> globals;

public:
    RuntimeInfo(string func, string file)
    {
    func_name = func;
    file_name = file;
    exec_times = 0;
    }

    ~RuntimeInfo(){};

    string getFuncName() { return func_name; }
    string getFileName() { return file_name; }
    unsigned long getExecTimes() { return exec_times; }
    vector<GlobalVariable *> getGlobals() { return globals; }

    void addGlobal(GlobalVariable *global_var) { globals.push_back(global_var); }
    void addExecTimes() { exec_times++; }

    void __attribute__((noinline)) dump()
    {
        cerr << "function name: " << func_name << endl;
        cerr << "file     name: " << file_name << endl;
        for (auto g_var : globals) {
            g_var->dump();
        }
    }
};

class RuntimeInfoManager {
private:
    bool enable_rt;
    stack<RuntimeInfo *> call_stack;
    vector<RuntimeInfo *> created_infos;

public:
    RuntimeInfoManager() { enable_rt = false; }
    ~RuntimeInfoManager();

    void setEnableRT(bool flag) { enable_rt = flag; }
    bool getEnableRT() { return enable_rt; }

    stack<RuntimeInfo *> getCallStack() { return call_stack; }
    void pushFunc(RuntimeInfo *func) { call_stack.push(func); }
    void popFunc() { call_stack.pop(); }

    void addRuntimeInfo(RuntimeInfo *rti) { created_infos.push_back(rti); }
    vector<RuntimeInfo *> getRuntimeInfos() { return created_infos; }
};

RuntimeInfoManager *RIM = nullptr;

void enable_runtime_info()
{
    if (!RIM) {
        RIM = new RuntimeInfoManager();
    }
    RIM->setEnableRT(true);
}

void disable_runtime_info()
{
    assert(RIM && "runtime manager not created.");
    if (RIM->getEnableRT()) {
    if (RIM->getCallStack().empty()) {
        RIM->setEnableRT(false);
    }
    }
}

// `*runtime_ptr` stores pointer to function runtime info.
// compiler allocates a global pointer for each function
// to reduce runtime info query time. `*runtime_ptr` is
// initialized to 0 and only assigned once during first execution of each function.
void enter_func(const char *func, const char *file,
                unsigned long *runtime_ptr)
{
    if (!RIM || !(RIM->getEnableRT())) {
        return;
    }
    RuntimeInfo *rti = nullptr;
    if (*runtime_ptr == 0) {
        rti = new RuntimeInfo(func, file);
        *runtime_ptr = reinterpret_cast<unsigned long>(rti);
        RIM->addRuntimeInfo(rti);
    } else {
        rti = reinterpret_cast<RuntimeInfo *>(*runtime_ptr);
    }
    rti->addExecTimes();
    RIM->pushFunc(rti);
}

void exit_func()
{
    if (!RIM || !(RIM->getEnableRT())) {
        return;
    }
    RIM->popFunc();
}

void create_global_symbol(const char *global, const unsigned long addr,
                          unsigned long *runtime_ptr)
{
    if (!RIM || !(RIM->getEnableRT())) {
        return;
    }
    RuntimeInfo *rti = reinterpret_cast<RuntimeInfo *>(*runtime_ptr);
    if (rti && rti->getExecTimes() == 1) {
        GlobalVariable *global_var = new GlobalVariable(global, addr);
        rti->addGlobal(global_var);
    }
}

void convertRTInfo2Json(RuntimeInfo *rti, ordered_json &ji)
{
    ji["func_name"] = rti->getFuncName();
    ji["file_name"] = rti->getFileName();
    ji["globals"] = ordered_json::array();
    for (auto g_var : rti->getGlobals()) {
        ordered_json g_var_json = ordered_json::object();
        g_var_json["global_name"] = g_var->getName();
        g_var_json["global_addr"] = g_var->getAddress();
        ji["globals"].push_back(g_var_json);
    }
}

// Write total runtime info to file when main function exits, instead of kernel
// function exiting. Because kernel function may be invoked many times and I/O
// would be slow in the later times when the file is too large.
void write_file()
{
    if (RIM) {
        ordered_json json_info = ordered_json::array();
        for (auto rt_info : RIM->getRuntimeInfos()) {
            ordered_json func_info = ordered_json::object();
            convertRTInfo2Json(rt_info, func_info);
            json_info.push_back(func_info);
        }
        std::ofstream outfile;
        outfile.open("linx_runtime_info.json", ios::out);
        outfile << setw(2) << json_info;
        outfile.close();
    }
}

// Get actual host binary path from symbolic link
string get_host_binary_path()
{
    constexpr size_t MAX_PATH_LENGTH = 100;
    char result[MAX_PATH_LENGTH];
    ssize_t count = readlink("/proc/self/exe", result, MAX_PATH_LENGTH);
    return string(result, (count > 0) ? count : 0);
}

void PCCache::initialize()
{
    if (initialized) {
        return;
    }
    string host_elf_str = get_host_binary_path();
    char host_elf[host_elf_str.size() + 1];
    strcpy(host_elf, host_elf_str.c_str());
    const char *device_elf = std::getenv("DEVICEELF");
    if (!device_elf) {
        std::cout << "WARNING: env parameter DEVICEELF unset, using default path: bisa_kernel.o" << std::endl;
        device_elf = "bisa_kernel.o";
    }
    SymbolTable device_symbol{};
    SymbolTable host_symbol{};
    cache_symbol(device_elf, device_symbol);
    cache_symbol(host_elf, host_symbol);
    cache_pc_map(device_symbol, host_symbol);
    initialized = true;
}

int PCCache::check_file_and_version(const char *file_path)
{
    int fd;
    if ((fd = open(file_path, O_RDONLY)) < 0) {
        cout << "ERROR: opening file: " << file_path << " failed." << endl;
        exit(EXIT_FAILURE);
    }
    if (elf_version(EV_CURRENT) == EV_NONE) {
        cout << "WARNING: ELF Library is out of date!" << endl;
        exit(EXIT_FAILURE);
    }
    return fd;
}

void PCCache::cache_pc_map(const SymbolTable& device_symbol, const SymbolTable& host_symbol)
{
    for (const auto &pair : device_symbol) {
        if (host_symbol.find(pair.first) == host_symbol.end()) {
        continue;
    }
    const uint64_t device_pc = pair.second;
    const uint64_t host_pc = host_symbol.at(pair.first);
    device_host_map->emplace(std::make_pair(device_pc, host_pc));
    host_device_map->emplace(std::make_pair(host_pc, device_pc));
    }
}

uint64_t PCCache::get_pc_from_cache(uint64_t src_pc, bool host_caller)
{
    if (host_caller) {
        if (device_host_map->find(src_pc) == device_host_map->end()) {
            cout << "ERROR: Cannot find PC: " << src_pc << "in device binary." << endl;
            exit(EXIT_FAILURE);
        }
        return device_host_map->at(src_pc);
    } else {
        if (host_device_map->find(src_pc) == host_device_map->end()) {
            cout << "WARNING: Cannot find PC: " << src_pc
            << " in host binary, maybe it is a device pc." << endl;
        return src_pc;
        }
        return host_device_map->at(src_pc);
    }
}

void PCCache::cache_symbol(const char* elf_name, SymbolTable& symbol_table)
{
    Elf_Scn *scn; /* section descriptor pointer */
    Elf_Data *data;
    Elf64_Shdr *shdr;
    int fd = check_file_and_version(elf_name);
    /* Initialize elf pointer for examining contents of file */
    Elf *elf = elf_begin(fd, ELF_C_READ, nullptr);
    /* Initialize section descriptor pointer so that elf_nextscn()
    * returns a pointer to the section descriptor at index 1. */
    scn = nullptr;
    /* Iterate through ELF sections */
    while ((scn = elf_nextscn(elf, scn)) != nullptr) {
        /* Retrieve section header */
        shdr = elf64_getshdr(scn);
        if (shdr->sh_type == SHT_SYMTAB) {
            data = elf_getdata(scn, nullptr);
            int count = shdr->sh_size / shdr->sh_entsize;
            for (int ii = 0; ii < count; ii++) {
            GElf_Sym sym;
            gelf_getsym(data, ii, &sym);
            std::string name(elf_strptr(elf, shdr->sh_link, sym.st_name));
            symbol_table.emplace(make_pair(name, sym.st_value));
            }
        }
    }
}

uint64_t PCCache::lookup_tgt_pc(uint64_t src_pc, bool host_caller)
{
    uint64_t tgt_pc;
    if (host_caller) {
        if (src_pc & 0x8000000000000000) {
            src_pc = src_pc & 0x7fffffffffffffff;
            tgt_pc = get_pc_from_cache(src_pc, host_caller);
            return tgt_pc;
        } else {
            return src_pc;
        }
    } else {
        if (src_pc & 0x8000000000000000) {
            src_pc = src_pc & 0x7fffffffffffffff;
            return src_pc;
        } else {
            tgt_pc = get_pc_from_cache(src_pc, host_caller);
            return tgt_pc;
        }
    }
}

nlohmann::ordered_json PCCache::dump_pc_table()
{
    nlohmann::ordered_json pc_table = nlohmann::ordered_json::object();
    nlohmann::ordered_json pc_table_array = nlohmann::ordered_json::array();
    for (auto pair : *host_device_map) {
        pc_table["host"] = pair.first;
        pc_table["device"] = pair.second;
        pc_table_array.push_back(pc_table);
    }
    return pc_table_array;
}

void PCCache::InitializeFromRjson(rapidjson::Document& doc)
{
    rapidjson::Value& root = doc[0];
    assert(root.IsObject() && "The second layer of ckpt is an object!");

    if (root.HasMember("pc_table") && root["pc_table"].IsArray()) {
        rapidjson::Value& pcTable = root["pc_table"];
        for (rapidjson::SizeType i = 0; i < pcTable.Size(); i++) {
            rapidjson::Value& pcTableVal = pcTable[i];
            auto host_pc = static_cast<uint64_t>(pcTableVal["host"].GetUint64());
            auto device_pc = static_cast<uint64_t>(pcTableVal["device"].GetUint64());
            (*host_device_map)[host_pc] = device_pc;
            (*device_host_map)[device_pc] = host_pc;
        }
        initialized = true;
    }
}

std::shared_ptr<PCMap> PCCache::host_device_map = std::make_shared<PCMap>();
std::shared_ptr<PCMap> PCCache::device_host_map = std::make_shared<PCMap>();
bool PCCache::initialized = false;

uint64_t get_target_pc(uint64_t pc, bool host_caller)
{
    char* model_running = std::getenv("BISA_MODEL_RUN");
    if (model_running == nullptr || atoi(model_running) == 0) {
        return pc;
    }
    PCCache::initialize();
    return PCCache::lookup_tgt_pc(pc, host_caller);
}
