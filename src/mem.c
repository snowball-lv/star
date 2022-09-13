#include <star/mem.h>
#include <stdlib.h>
#include <stdio.h>

static int _allocated = 0;

typedef struct {
    int size;
    long double _align[];
} Hdr;

void *xmalloc(int size) {
    Hdr *hdr = malloc(sizeof(Hdr) + size);
    hdr->size = size;
    _allocated += size;
    return hdr + 1;
}

void *xrealloc(void *ptr, int size) {
    Hdr *hdr = (Hdr *)ptr - 1;
    _allocated -= hdr->size;
    hdr = realloc(hdr, sizeof(Hdr) + size);
    _allocated += size;
    hdr->size = size;
    return hdr + 1;
}

void xfree(void *ptr) {
    Hdr *hdr = (Hdr *)ptr - 1;
    _allocated -= hdr->size;
    free(hdr);
}

void printmem() {
    printf("%i bytes allocated\n", _allocated);
}
