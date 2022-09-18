#include <string.h>
#include <star/mem.h>
#include <star/util.h>

typedef struct {
    int elemsz;
    int cap;
    long double _align[];
} Array;

void *newarray(int elemsz) {
    Array *hdr = xmalloc(sizeof(Array) + 4 * elemsz);
    hdr->elemsz = elemsz;
    hdr->cap = 4;
    return hdr + 1;
}

void *arraygrow(void *array, int size) {
    Array *hdr = (Array *)array - 1;
    if (hdr->cap >= size) return array;
    while (hdr->cap < size) hdr->cap *= 2;
    hdr = xrealloc(hdr, sizeof(Array) + hdr->cap * hdr->elemsz);
    return hdr + 1;
}

void freearray(void *array) {
    Array *hdr = (Array *)array - 1;
    xfree(hdr);
}

typedef union {
    int i;
    void *ptr;
} TabVal;

struct Tab {
    char *key;
    TabVal val;
    Tab *next;
};

Tab *newtab() {
    Tab *t = xmalloc(sizeof(Tab));
    memset(t, 0, sizeof(Tab));
    return t;
}

void freetab(Tab *t) {
    Tab *n;
    while (t) {
        n = t->next;
        xfree(t);
        t = n;
    }
}

static Tab *tabfind(Tab *t, char *key) {
    for (t = t->next; t; t = t->next)
        if (strcmp(t->key, key) == 0) return t;
    return 0;
}

int tabhas(Tab *t, char *key) {
    return tabfind(t, key) != 0;
}

int tabget(Tab *t, char *key) {
    Tab *e = tabfind(t, key);
    return e ? e->val.i : 0;
}

void tabset(Tab *t, char *key, int val) {
    Tab *e = xmalloc(sizeof(Tab));
    e->key = key;
    e->val.i = val;
    e->next = t->next;
    t->next = e;
}

void *tabgetp(Tab *t, char *key) {
    Tab *e = tabfind(t, key);
    return e ? e->val.ptr : 0;
}

void tabsetp(Tab *t, char *key, void *ptr) {
    Tab *e = xmalloc(sizeof(Tab));
    e->key = key;
    e->val.ptr = ptr;
    e->next = t->next;
    t->next = e;
}

// https://stackoverflow.com/a/7666577
unsigned strhash(char *str) {
    int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}
