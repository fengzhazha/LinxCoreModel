#include <stdio.h>

#define THREAD_SHARE
#define THREAD_LOCAL

#include "llama.h"

void writeArraysToFile() {

    FILE *file = fopen("arrays.txt", "w");
    if (file == NULL) {
        printf("Failed to open file for writing.");
        return;
    }

    int size1 = sizeof(_node) / sizeof(_node[0]);
    int size2 = sizeof(_offset) / sizeof(_offset[0]);
    int size3 = sizeof(_pred) / sizeof(_pred[0]);
    int size4 = sizeof(_free_list) / sizeof(_free_list[0]);

    fprintf(file, "%d\n", size1);
    for (int i = 0; i < size1; i++) {
        fprintf(file, "%d ", _node[i]);
    }
    fprintf(file, "\n");

    fprintf(file, "%d\n", size2);
    for (int i = 0; i < size2; i++) {
        fprintf(file, "%d ", _offset[i]);
    }
    fprintf(file, "\n");

    fprintf(file, "%d\n", size3);
    for (int i = 0; i < size3; i++) {
        fprintf(file, "%d ", _pred[i]);
    }
    fprintf(file, "\n");

    fprintf(file, "%d\n", size4);
    for (int i = 0; i < size4; i++) {
        fprintf(file, "%d ", _free_list[i]);
    }
    fprintf(file, "\n");

    fclose(file);
}

int main() {
    writeArraysToFile();
    return 0;
}