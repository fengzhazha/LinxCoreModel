#ifndef __LINDART_LXDBG_H
#define __LINDART_LXDBG_H

// #define ENABLE_DBG_INSTR

#ifdef LINX6188
#include "sre_uart.h"
#endif

#ifdef ENABLE_DBG_INSTR

#ifdef __linx
#ifdef LINXV4
#define DUMP_VAR(id, var) \
  __asm__ __volatile__ (  \
    "dbg %0, %1\n"        \
    :                     \
    : "r"(var), "i"(id)   \
  );
#else // LINX6188
#define DUMP_VAR(id, var) SRE_printf("[%d] %lld\n", (id), (int64_t)(var))
#endif // LINXV4
#else
#define DUMP_VAR(id, var) printf("[%d] %lld\n", (id), (int64_t)(var));
#endif // __linx

#else
#define DUMP_VAR(id, var)
#endif // ENABLE_DBG_INSTR

#endif // __LINDART_LXDBG_H