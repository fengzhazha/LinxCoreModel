#ifndef __ELF_LOAD_H__
#define __ELF_LOAD_H__

#include <elf.h>
#include <fstream>
#include <iostream>
#include <stdint.h>

#include "document.h"
#include "istreamwrapper.h"
#include "json.hpp"
#include "Memory.h"
#include "stringbuffer.h"
#include "writer.h"

namespace JCore {

//-------------------------------------------------------------
// Types
//-------------------------------------------------------------
typedef int (*cb_mem_create)(void *arg, uint64_t base, uint64_t size);
typedef int (*cb_mem_load)(void *arg, uint64_t addr, uint8_t data);

//-------------------------------------------------------------
// Functions
//-------------------------------------------------------------
int elf_load(const char *filename, cb_mem_create fn_create, cb_mem_load fn_load, void *arg, uint64_t *start_addr,
             uint64_t *sp_addr, int argc = 0, const std::vector<std::string> &argv = {});

int  elf_load_ckpt(const char *filename, cb_mem_create fn_create, cb_mem_load fn_load, void *arg, uint64_t *start_addr, const char* func_name);

int  txt_load(const char *filename, cb_mem_create fn_create, cb_mem_load fn_load, void *arg, uint64_t *start_addr, uint64_t* regs, std::map<uint64_t, uint64_t>& sysregs, int imem_en, uint64_t* end_addr, uint64_t *execBlockCnt);


int load_txt(const char *filename, TxtArgs &txt_a);

int TxtLoadTxt(std::pair<cb_mem_create, cb_mem_load>& memVerify, JsonElement* jsonElement,
               std::map<uint64_t, uint64_t>& sysregs, TxtArgs& txtArgs);

} // namespace JCore

#endif
