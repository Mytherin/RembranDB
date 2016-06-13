
#include <stdio.h>
#include <string.h>
#include <malloc.h>

int main() {
    FILE *fp = fopen("Tables/benchmark/x.col", "r");
    size_t elcount = 250000;
    size_t elsize = sizeof(double);
    void *data = malloc(elcount * elsize);
    fread(data, elsize, elcount, fp);
    for(int i = 0; i < 10; i++) {
        printf("%g,", ((double*)data)[i]);
    }
    printf("\n");
}