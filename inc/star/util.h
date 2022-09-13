#pragma once

void *newarray(int elemsz);
void *arraygrow(void *array, int size);
void freearray(void *array);

typedef struct Tab Tab;

Tab *newtab();
void freetab(Tab *t);
int tabhas(Tab *t, char *key);
int tabget(Tab *t, char *key);
void tabset(Tab *t, char *key, int val);
void *tabgetp(Tab *t, char *key);
void tabsetp(Tab *t, char *key, void *ptr);

int strhash(char *str);
