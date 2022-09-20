#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <star/star.h>
#include <star/mem.h>

typedef struct Entry Entry;
struct Entry {
    ObjString *key;
    Value value;
};

struct ValTab {
    Entry *slots;
    int nslots;
    int nused;
};

int valtabnext(ValTab *vt, int idx, ObjString **key, Value *dst) {
    for (; idx < vt->nslots; idx++) {
        Entry *e = &vt->slots[idx];
        if (!e->key) continue;
        *key = e->key;
        *dst = e->value;
        return idx + 1;
    }
    return 0;
}

ValTab *newvaltab() {
    ValTab *vt = xmalloc(sizeof(ValTab));
    memset(vt, 0, sizeof(ValTab));
    return vt;
}

void freevaltab(ValTab *vt) {
    if (vt->slots) xfree(vt->slots);
    xfree(vt);
}

static int keycmp(ObjString *a, ObjString *b) {
    if (!a || !b) return 0;
    if (a->hash != b->hash) return 0;
    if (a->len != b->len) return 0;
    if (strcmp(a->str, b->str) != 0) return 0;
    return 1;
}

int valtabget(ValTab *vt, ObjString *key, Value *dst) {
    if (!vt->nslots) return 0;
    for (int i = 0; i < vt->nslots; i++) {
        int idx = (key->hash + i) % vt->nslots;
        Entry *e = &vt->slots[idx];
        if (!keycmp(e->key, key)) continue;
        *dst = e->value;
        return 1;
    }
    return 0;
}

void valtabset(ValTab *vt, ObjString *key, Value v);

static void grow(ValTab *vt) {
    ValTab old = *vt;
    vt->nslots = vt->nslots ? vt->nslots * 2 : 8;
    // printf("grow %i -> %i\n", old.nslots, vt->nslots);
    vt->slots = xmalloc(vt->nslots * sizeof(Entry));
    memset(vt->slots, 0, vt->nslots * sizeof(Entry));
    vt->nused = 0;
    for (int i = 0; i < old.nslots; i++) {
        Entry *e = &old.slots[i];
        if (!e->key) continue;
        valtabset(vt, e->key, e->value);
    }
    if (old.slots) xfree(old.slots);
}

void valtabset(ValTab *vt, ObjString *key, Value v) {
    if (vt->nused + 1 > vt->nslots / 2)
        grow(vt);
    for (int i = 0; i < vt->nslots; i++) {
        int idx = (key->hash + i) % vt->nslots;
        Entry *e = &vt->slots[idx];
        if (e->key && !keycmp(e->key, key)) continue;
        if (!e->key) vt->nused++;
        // printf("set at +%i\n", i);
        e->key = key;
        e->value = v;
        return;
    }
    printf("*** out of free slots\n");
    exit(1);
}
