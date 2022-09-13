#pragma once

void *xmalloc(int size);
void *xrealloc(void *ptr, int size);
void xfree(void *ptr);

void printmem();
