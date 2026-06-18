    .global  _start
    .type   _start,@function
    .text
_start:
    c.bstart.std call main
    .Ltmp0:
    addtpc %tpcrel_hi(.SRC_GQM_ADDR), ->t
    addi t#1, %tpcrel_lo(.Ltmp0), ->a1
    .Ltmp1:
    addtpc %tpcrel_hi(.DST_GQM_ADDR), ->t
    addi t#1, %tpcrel_lo(.Ltmp1), ->a2

_end:
  c.bstart.std fall
  addi zero, 0x5e -> x1
  bstart.ecall

.pushsection .data
.SRC_GQM_ADDR:
    .dword 0x0
    .dword 0x4
    .dword 0x1
    .dword 0x2
    .dword 0x3

.DST_GQM_ADDR:
    .dword 0x0
    .dword 0x1
    .dword 0x2
    .dword 0x3
    .dword 0x4