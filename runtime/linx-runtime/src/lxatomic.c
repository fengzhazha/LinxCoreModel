#include "lxatomic.h"

#include <assert.h>
#ifndef __linx
#include <pthread.h>
#endif

int32_t atomic_load_add_4(int32_t *src, int32_t val, int model) {
#ifdef __linx
#ifdef LINXV4
  int32_t rval;
  switch (model) {
    case 0:
      __asm__ __volatile__ (
        "C.BSTART.SYS\n"
        "lw.add [%[src]], %[val], -> %[rval]\n"
        : [rval]"=r"(rval)
        : [src]"r"(src), [val]"r"(val)
      );
      break;
    case 1:
      __asm__ __volatile__ (
        "C.BSTART.SYS\n"
        "lw.add.aq [%[src]], %[val], -> %[rval]\n"
        : [rval]"=r"(rval)
        : [src]"r"(src), [val]"r"(val)
      );
      break;
    case 2:
      __asm__ __volatile__ (
        "C.BSTART.SYS\n"
        "lw.add.rl [%[src]], %[val], -> %[rval]\n"
        : [rval]"=r"(rval)
        : [src]"r"(src), [val]"r"(val)
      );
      break;
    case 3:
      __asm__ __volatile__ (
        "C.BSTART.SYS\n"
        "lw.add.aqrl [%[src]], %[val], -> %[rval]\n"
        : [rval]"=r"(rval)
        : [src]"r"(src), [val]"r"(val)
      );
      break;
    default:
      assert(0 && "Invalid acquire/release mode!\n");
  }
  return rval;
#else // LINX6188
  model = model + 1;
  S32 afterAdd = SRE_AtomicAdd32Rtn((S32*)src, val);
  return afterAdd - val;
#endif
#else  // undefine __linx
    return __sync_fetch_and_add(src, val);
#endif
}

int64_t atomic_load_add_8(int64_t *src, int64_t val, int model) {
#ifdef __linx
#ifdef LINXV4
  int64_t rval;
  switch (model) {
    case 0:
      __asm__ __volatile__ (
        "C.BSTART.SYS\n"
        "ld.add [%[src]], %[val], -> %[rval]\n"
        : [rval]"=r"(rval)
        : [src]"r"(src), [val]"r"(val)
      );
      break;
    case 1:
      __asm__ __volatile__ (
        "C.BSTART.SYS\n"
        "ld.add.aq [%[src]], %[val], -> %[rval]\n"
        : [rval]"=r"(rval)
        : [src]"r"(src), [val]"r"(val)
      );
      break;
    case 2:
      __asm__ __volatile__ (
        "C.BSTART.SYS\n"
        "ld.add.rl [%[src]], %[val], -> %[rval]\n"
        : [rval]"=r"(rval)
        : [src]"r"(src), [val]"r"(val)
      );
      break;
    case 3:
      __asm__ __volatile__ (
        "C.BSTART.SYS\n"
        "ld.add.aqrl [%[src]], %[val], -> %[rval]\n"
        : [rval]"=r"(rval)
        : [src]"r"(src), [val]"r"(val)
      );
      break;
    default:
      assert(0 && "Invalid acquire/release mode!\n");
  }
  return rval;
#else // LINX6188
  model = model + 1;
  S64 afterAdd = SRE_AtomicAdd64Rtn((S64*)src, val);
  return afterAdd - val;
#endif
#else // undefine __linx
  return __sync_fetch_and_add(src, val);
#endif
}

// void atomic_store_add_4(int32_t *dest, int32_t val, int model) {
// #ifdef __linx
//   switch (model) {
//     case 0:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "sw.add [%[dest]], %[val]\n"
//         :
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 2:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "sw.add.rl [%[dest]], %[val]\n"
//         :
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     default:
//       assert(0 && "Invalid acquire/release mode!\n");
//   }
// #else
//   assert(0 && "Not supported in general architecture!");
// #endif
// }

// void atomic_store_add_8(int64_t *dest, int64_t val, int model) {
// #ifdef __linx
//   switch (model) {
//     case 0:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "sd.add [%[dest]], %[val]\n"
//         :
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 2:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "sd.add.rl [%[dest]], %[val]\n"
//         :
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     default:
//       assert(0 && "Invalid acquire/release mode!\n");
//   }
// #else
//   assert(0 && "Not supported in general architecture!");
// #endif
// }

// uint8_t atomic_exchange_1(uint8_t *dest, uint8_t val, int model) {
// #ifdef __linx
//   uint8_t rval;
//   switch (model) {
//     case 0:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapb [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 1:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapb.aq [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 2:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapb.rl [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 3:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapb.aqrl [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     default:
//       assert(0 && "Invalid acquire/release mode!\n");
//   }
//   return rval;
// #else
//   assert(0 && "Not supported in general architecture!");
//   return 0;
// #endif
// }

// uint16_t atomic_exchange_2(uint16_t *dest, uint16_t val, int model) {
// #ifdef __linx
//   uint16_t rval;
//   switch (model) {
//     case 0:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swaph [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 1:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swaph.aq [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 2:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swaph.rl [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 3:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swaph.aqrl [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     default:
//       assert(0 && "Invalid acquire/release mode!\n");
//   }
//   return rval;
// #else
//   assert(0 && "Not supported in general architecture!");
//   return 0;
// #endif
// }

// uint32_t atomic_exchange_4(uint32_t *dest, uint32_t val, int model) {
// #ifdef __linx
//   uint32_t rval;
//   switch (model) {
//     case 0:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapw [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 1:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapw.aq [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 2:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapw.rl [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 3:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapw.aqrl [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     default:
//       assert(0 && "Invalid acquire/release mode!\n");
//   }
//   return rval;
// #else
//   assert(0 && "Not supported in general architecture!");
//   return 0;
// #endif
// }

// uint64_t atomic_exchange_8(uint64_t *dest, uint64_t val, int model) {
// #ifdef __linx
//   uint64_t rval;
//   switch (model) {
//     case 0:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapd [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 1:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapd.aq [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 2:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapd.rl [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     case 3:
//       __asm__ __volatile__ (
//         "C.BSTART.SYS\n"
//         "swapd.aqrl [%[dest]], %[val], -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [dest]"r"(dest), [val]"r"(val)
//       );
//       break;
//     default:
//       assert(0 && "Invalid acquire/release mode!\n");
//   }
//   return rval;
// #else
//   assert(0 && "Not supported in general architecture!");
//   return 0;
// #endif
// }

// if old_data == *dest, *dest = new_data
// return old *dest
bool compare_and_swap_4(uint32_t *dest, uint32_t old_data, uint32_t new_data) {
#ifdef __linx
#ifdef LINXV4
  uint32_t old_backup = old_data;
  __asm__ __volatile__ (
    "C.BSTART.SYS\n"
    "casw.aqrl [%[dest]], %[newd], -> %[old]\n"
    : [old]"+r"(old_data)
    : [dest]"r"(dest), [newd]"r"(new_data)
  );
  return old_data == old_backup;
#else // LINX6188
  return SRE_AtomicCompareAndStore32(dest, old_data, new_data);
#endif
#else // undefine __linx
  return __sync_bool_compare_and_swap(dest, old_data, new_data);
#endif
}

// if old_data == *dest, *dest = new_data
// return old *dest
bool compare_and_swap_8(uint64_t *dest, uint64_t old_data, uint64_t new_data) {
#ifdef __linx
#ifdef LINXV4
  uint64_t old_backup = old_data;
  __asm__ __volatile__ (
    "C.BSTART.SYS\n"
    "casd.aqrl [%[dest]], %[newd], -> %[old]\n"
    : [old]"+r"(old_data)
    : [dest]"r"(dest), [newd]"r"(new_data)
  );
  return old_data == old_backup;
#else // LINX6188
  return SRE_AtomicCompareAndStore64((U64*)dest, old_data, new_data);
#endif
#else // undefined __linx
  return __sync_bool_compare_and_swap(dest, old_data, new_data);
#endif
}

void init_lock(__volatile__ lock_t *l) {
  l->flag = 0;
}

// void lock(__volatile__ lock_t *l) {
//   __asm__ __volatile__ (
//     "C.BSTART.SYS\n"
//     ".L_try_lock:\n"
//     "c.movr zero, -> a1\n"
//     "c.movi, 1, -> a2\n"
//     "casw.aqrl [a0], a2, -> a1\n"
//     "b.ne a1, zero, .L_try_lock\n"
//   );
// }

void unlock(__volatile__ lock_t *l) {
  l->flag = 0;
}