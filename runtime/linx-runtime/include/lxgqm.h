#ifndef __LINDART_LXGQM_H
#define __LINDART_LXGQM_H

#include <stdlib.h>
#include <stdint.h>

#ifndef __linx
#include "concurrent_queue.h"
#endif

#ifdef LINX6188
#include "drb_qm.h"
#include "sre_uart.h"
#endif

#define PHY_PAGE_SIZE 8192

typedef enum {
  GQM_RESUME  = 0,
  GQM_PENDING = 1,
} gqm_state;

typedef enum {
  GQM_SUACCELSS   = 0,
  GQM_FULL      = 1,
  GQM_EMPTY     = 2,
  GQM_CORRUPTED = 3,
  GQM_FAIL      = 4,
} gqm_errno;

typedef enum {
  GQM_NO_CTRL = 0x0,
  GQM_RELAX_ORDER = 0x1,
  GQM_AWAKEN = 0x2,
} gqm_ctrl;

typedef struct {
#ifdef __linx
#ifdef LINXV4
  void* addr;
  size_t capacity;
  gqm_state state;
#else // LINX6188
  U32 qid;
#endif
#else
  queue_t queue;
#endif
} gqm_t;

typedef uint64_t item_t;

/** @brief Initialize the GQM with specific capacity.
 *  
 *  The memory space of GQM is allocated on the heap.
 *  TODO: switch to physical page when porting to Linux 
 *
 *  @param ctx The GQM data structure.
 *  @param capacity The desired capacity of the GQM. It should
 *                  not exceed 1023.
 *  @return Error number.
 */
int gqm_init(gqm_t* ctx, size_t capacity);

/** @brief Destroy the GQM.
 *  
 *  The memory space of GQM is freed. The capacity is set to 0.
 *
 *  @param ctx The GQM data structure.
 *  @return Error number.
 */
int gqm_destroy(gqm_t* ctx);

/** @brief Push data item to the tail of the GQM.
 *  
 *  Each item is treated as a 64-bit data. The side-effect when pushing
 *  is set by the parameter `ctrl`.
 *    GQM_RELAX_ORDER: relax the memory order. If not set, all memory 
 *                     write operations are guaranteed to be observed 
 *                     before the data is popped.
 *    GQM_AWAKEN: awaken other cores which are on WFE status.
 *  GQM_RELAX_ORDER and GQM_AWAKEN can be simultaneously set by 
 *  `GQM_RELAX_ORDER | GQM_AWAKEN`.
 *
 *  @param ctx The GQM data structure.
 *  @param data The 64-bit data item.
 *  @param ctrl Control the side-effect of the push.
 *  @return Error number.
 */
int gqm_push(gqm_t* ctx, item_t data, uint8_t ctrl);

/** @brief Push data item to the *head* of the GQM.
 *
 *  @param ctx The GQM data structure.
 *  @param data The 64-bit data item.
 *  @param ctrl Control the side-effect of the push.
 *  @return Error number.
 */
int gqm_push_head(gqm_t* ctx, item_t data, uint8_t ctrl);

/** @brief Pop data item from the head of the GQM.
 *
 *  @param ctx The GQM data structure.
 *  @param data The 64-bit data item popped from the GQM.
 *              Only overwritten when pop suaccelss.
 *  @param ctrl Control the side-effect of the push.
 *  @return Error number.
 */
int gqm_pop(gqm_t* ctx, item_t* data, uint8_t ctrl);

/** @brief Query current size of the GQM.
 *
 *  @param ctx The GQM data structure.
 *  @return Current size of the GQM.
 */
size_t gqm_size(gqm_t* ctx);

#endif // __LINDART_LXGQM_H