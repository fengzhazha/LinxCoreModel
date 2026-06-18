#include <stdio.h>

#include "graph_bfs.h"

int main(int argc, char* argv[]) {
  __asm__ __volatile__ (
    "C.BSTART\n"
    "ssrget 0x21, -> t\n"
    "slli t#1, 23, -> t\n"
    "add sp, t#1, -> sp\n"
  );
  run_bfs_nworker();
}
