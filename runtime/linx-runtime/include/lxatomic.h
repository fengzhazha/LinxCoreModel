#ifndef __LINDART_LXATOMIC_H
#define __LINDART_LXATOMIC_H

#include <stdint.h>
#include <stdbool.h>
#include "common.h"

#ifdef LINX6188
#include "drb_qm.h"
#include "sre_atomic.h"
#include "sre_atomic64.h"
#include "sre_uart.h"
#endif

typedef struct {
  int flag;
} lock_t;

enum {
  ATOMIC_ACQUIRE = 1,
  ATOMIC_RELEASE = 2,
};

// return: the original value in src
int32_t atomic_load_add_4(int32_t *src, int32_t val, int model);
int64_t atomic_load_add_8(int64_t *src, int64_t val, int model);

// void atomic_store_add_4(int32_t *dest, int32_t val, int model);
// void atomic_store_add_8(int64_t *dest, int64_t val, int model);

// uint8_t  atomic_exchange_1(uint8_t  *dest, uint8_t val,  int model);
// uint16_t atomic_exchange_2(uint16_t *dest, uint16_t val, int model);
// uint32_t atomic_exchange_4(uint32_t *dest, uint32_t val, int model);
// uint64_t atomic_exchange_8(uint64_t *dest, uint64_t val, int model);

bool compare_and_swap_4(uint32_t *dest, uint32_t old_data, uint32_t new_data);
bool compare_and_swap_8(uint64_t *dest, uint64_t old_data, uint64_t new_data);

void init_lock(__volatile__ lock_t *l);

// void lock(__volatile__ lock_t *l);

void unlock(__volatile__ lock_t *l);

#endif // __LINDART_LXATOMIC_H