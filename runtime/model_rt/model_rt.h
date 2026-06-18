#include <unordered_map>
#include <memory>
#include <cstdlib>
#include <gelf.h>
#include <libelf.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include "json.hpp"
#include "document.h"
#include "stringbuffer.h"
#include "writer.h"
#include "istreamwrapper.h"

#ifndef INCLUDE_LINX_RUNTIME_HPP
#define INCLUDE_LINX_RUNTIME_HPP

using SymbolTable = std::unordered_map<std::string, uint64_t>;
using PCMap = std::unordered_map<uint64_t, uint64_t>;

#ifdef __cplusplus
extern "C" {
#endif
void enable_runtime_info();

void disable_runtime_info();

void enter_func(const char *func_name, const char *file,
                unsigned long *runtime_ptr);

void exit_func();

void create_global_symbol(const char *global, const unsigned long addr,
                          unsigned long *runtime_ptr);

void write_file();

class PCCache {
private:
  static std::shared_ptr<PCMap> host_device_map;
  static std::shared_ptr<PCMap> device_host_map;
  static bool initialized;
  
  static int check_file_and_version(const char *file_path);

  static void cache_pc_map(const SymbolTable& device_symbol, const SymbolTable& host_symbol);
 
  static uint64_t get_pc_from_cache(uint64_t src_pc, bool host_caller);

  static void cache_symbol(const char* elf_name, SymbolTable& symbol_table);

public:
  static void initialize();

  static nlohmann::ordered_json dump_pc_table();

  static void InitializeFromRjson(rapidjson::Document& doc);
  
  static uint64_t lookup_tgt_pc(uint64_t src_pc, bool host_caller);

  static bool is_initialized() {
    return initialized;
  }
};

uint64_t get_target_pc(uint64_t pc, bool host_caller);

#ifdef __cplusplus
}
#endif

#endif
