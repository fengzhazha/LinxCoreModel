#include "lxgqm.h"

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#ifdef LINX6188
struct GQMItem {
  U32 data;
  U8 reserved[12];
};
#endif

int gqm_init(gqm_t* ctx, size_t capacity) {
#ifdef __linx
#ifdef LINXV4
  size_t mem_size = (capacity + 1) * sizeof(item_t);
  if (mem_size > PHY_PAGE_SIZE)
    return GQM_FAIL;
  ctx->addr = malloc(mem_size);
  uint64_t rval;
  __asm__ __volatile__ (
    "qmt.i %[addr], %[cap], -> t\n"
    "ori t#1, 0, -> %[rval]\n"
    : [rval]"=r"(rval)
    : [addr]"r"(ctx->addr), [cap]"r"(capacity)
  );
  switch ((rval >> 62) & 0x3) {
    case 0:
      ctx->capacity = rval & 0x3FF;
      return GQM_SUACCELSS;
    case 1:
      return GQM_CORRUPTED;
    default:
      return GQM_FAIL;
  }
#else // LINX6188
  capacity = capacity + 1;
  U32 ret = DRB_QmCreate(QM_SRAM_FIRST, &ctx->qid);
  if (ret == DRB_OK)
    return GQM_SUACCELSS;
  else
    return GQM_FAIL;
#endif
#else // undefine __linx
  queue_init(&ctx->queue);
  return GQM_SUACCELSS;
#endif
}

int gqm_destroy(gqm_t* ctx) {
#ifdef __linx
#ifdef LINXV4
  free(ctx->addr);
  ctx->capacity = 0;
  return GQM_SUACCELSS;
#else // LINX6188
  U32 ret = DRB_QmDelete(ctx->qid);
  if (ret == DRB_OK)
    return GQM_SUACCELSS;
  else
    return GQM_FAIL;
#endif
#else // __linx
  queue_destroy(&ctx->queue);
  return GQM_SUACCELSS;
#endif
}

int gqm_push(gqm_t* ctx, item_t data, uint8_t ctrl) {
#ifdef __linx
#ifdef LINXV4
  uint64_t rval;
  switch (ctrl) {
    case GQM_AWAKEN:
      __asm__ __volatile__ (
        "qpush.e %[addr], %[d], -> t\n"
        "ori t#1, 0, -> %[rval]\n"
        : [rval]"=r"(rval)
        : [addr]"r"(ctx->addr), [d]"r"(data)
      );
      break;
    case GQM_RELAX_ORDER:
      __asm__ __volatile__ (
        "qpush.r %[addr], %[d], -> t\n"
        "ori t#1, 0, -> %[rval]\n"
        : [rval]"=r"(rval)
        : [addr]"r"(ctx->addr), [d]"r"(data)
      );
      break;
    case GQM_RELAX_ORDER | GQM_AWAKEN:
      __asm__ __volatile__ (
        "qpush.re %[addr], %[d], -> t\n"
        "ori t#1, 0, -> %[rval]\n"
        : [rval]"=r"(rval)
        : [addr]"r"(ctx->addr), [d]"r"(data)
      );
      break;
    default:
      __asm__ __volatile__ (
        "qpush %[addr], %[d], -> t\n"
        "ori t#1, 0, -> %[rval]\n"
        : [rval]"=r"(rval)
        : [addr]"r"(ctx->addr), [d]"r"(data)
      );
      break;
  }
  switch ((rval >> 62) & 0x3) {
    case 0:
      return GQM_SUACCELSS;
    case 1:
      return GQM_CORRUPTED;
    case 2:
      return GQM_FULL;
    default:
      return GQM_FAIL;
  }
#else // LINX6188
  ctrl = ctrl + 1;
  struct GQMItem item = {
    .data = data,
  };
  U32 ret = DRB_QmPush(ctx->qid, (struct QmNodeInfo *)&item);
  if (ret == DRB_OK)
    return GQM_SUACCELSS;
  else
    return GQM_FAIL;
#endif
#else // undefine __linx
  queue_enqueue(&ctx->queue, data);
  return GQM_SUACCELSS;
#endif
}

// int gqm_push_head(gqm_t* ctx, item_t data, uint8_t ctrl) {
// #ifdef __linx
//   uint64_t rval;
//   switch (ctrl) {
//     case GQM_AWAKEN:
//       __asm__ __volatile__ (
//         "qpush.he %[addr], %[d], -> t\n"
//         "ori t#1, 0, -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [addr]"r"(ctx->addr), [d]"r"(data)
//       );
//       break;
//     case GQM_RELAX_ORDER:
//       __asm__ __volatile__ (
//         "qpush.hr %[addr], %[d], -> t\n"
//         "ori t#1, 0, -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [addr]"r"(ctx->addr), [d]"r"(data)
//       );
//       break;
//     case GQM_RELAX_ORDER | GQM_AWAKEN:
//       __asm__ __volatile__ (
//         "qpush.hre %[addr], %[d], -> t\n"
//         "ori t#1, 0, -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [addr]"r"(ctx->addr), [d]"r"(data)
//       );
//       break;
//     default:
//       __asm__ __volatile__ (
//         "qpush.h %[addr], %[d], -> t\n"
//         "ori t#1, 0, -> %[rval]\n"
//         : [rval]"=r"(rval)
//         : [addr]"r"(ctx->addr), [d]"r"(data)
//       );
//       break;
//   }
//   switch ((rval >> 62) & 0x3) {
//     case 0:
//       return GQM_SUACCELSS;
//     case 1:
//       return GQM_CORRUPTED;
//     case 2:
//       return GQM_FULL;
//     default:
//       return GQM_FAIL;
//   }
// #else
//   assert(0 && "Not supported in general architecture!");
//   return 0;
// #endif
// }

int gqm_pop(gqm_t* ctx, item_t* data, uint8_t ctrl) {
#ifdef __linx
#ifdef LINXV4
  uint64_t rval;
  item_t popped;
  switch (ctrl) {
    case GQM_AWAKEN:
      __asm__ __volatile__ (
        "qpop.e %[addr], -> tx2\n"
        "ori t#1, 0, -> %[rval]\n"
        "ori t#2, 0, -> %[d]\n"
        : [rval]"=r"(rval), [d]"=r"(popped)
        : [addr]"r"(ctx->addr)
      );
      break;
    case GQM_RELAX_ORDER:
      __asm__ __volatile__ (
        "qpop.r %[addr], -> tx2\n"
        "ori t#1, 0, -> %[rval]\n"
        "ori t#2, 0, -> %[d]\n"
        : [rval]"=r"(rval), [d]"=r"(popped)
        : [addr]"r"(ctx->addr)
      );
      break;
    case GQM_RELAX_ORDER | GQM_AWAKEN:
      __asm__ __volatile__ (
        "qpop.re %[addr], -> tx2\n"
        "ori t#1, 0, -> %[rval]\n"
        "ori t#2, 0, -> %[d]\n"
        : [rval]"=r"(rval), [d]"=r"(popped)
        : [addr]"r"(ctx->addr)
      );
      break;
    default:
      __asm__ __volatile__ (
        "qpop %[addr], -> tx2\n"
        "ori t#1, 0, -> %[rval]\n"
        "ori t#2, 0, -> %[d]\n"
        : [rval]"=r"(rval), [d]"=r"(popped)
        : [addr]"r"(ctx->addr)
      );
      break;
  }
  switch ((rval >> 62) & 0x3) {
    case 0:
      *data = popped;
      return GQM_SUACCELSS;
    case 1:
      return GQM_CORRUPTED;
    case 2:
      return GQM_EMPTY;
    default:
      return GQM_FAIL;
  }
#else // LINX6188
  ctrl = ctrl + 1;
  struct QmPopData popped;
  U32 remains;
  U32 ret = DRB_QmPopN(ctx->qid, 1, &popped, &remains);   // this API return DRB_OK even if nothing popped
  // SRE_printf("POP num: %d, data: %d\n", popped.nodeNum, popped.node[0].node[0]);
  if (ret == DRB_OK && popped.nodeNum == 1) {
    struct GQMItem *item = (struct GQMItem *)&(popped.node[0]);
    *data = item->data;
    return GQM_SUACCELSS;
  } else {
    return GQM_FAIL;
  }
#endif
#else // undefine __linx
  uint64_t temp = queue_dequeue(&ctx->queue);
  if (temp == (uint64_t)-1) {
    return GQM_EMPTY;
  } else {
    *data = temp;
    return GQM_SUACCELSS;
  }

#endif
}

size_t gqm_size(gqm_t* ctx) {
#ifdef __linx
#ifdef LINXV4
  uint64_t rval;
  __asm__ __volatile__ (
    "qmt %[addr], t#1, -> t\n"
    "ori t#1, 0, -> %[rval]\n"
    : [rval]"=r"(rval)
    : [addr]"r"(ctx->addr)
  );
  assert((((rval >> 62) & 0x3) == 0) && "Get GQM size failed!\n");
  return rval & 0x3FF;
#else // LINX6188
  assert(0);
  return ctx->qid;
#endif
#else // undefine __linx
  return queue_size(&ctx->queue);
#endif
}