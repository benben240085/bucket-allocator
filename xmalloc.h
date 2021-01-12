#ifndef XMALLOC_H
#define XMALLOC_H

#include <stddef.h>

void* xmalloc(size_t size);
void  xfree(void* item);
void* xrealloc(void* prev, size_t bytes);

#endif
