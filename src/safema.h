#ifndef __SAFE_MALLOC_H
#define __SAFE_MALLOC_H

#define _SM_MALLOC_FAIL -10
#define _SM_CALLOC_FAIL -11
#define _SM_REALLOC_FAIL -12

#include <stdlib.h>

void SMerror(int ec);

void *SMsafeMalloc(size_t size);

void *SMsafeCalloc(size_t size, size_t unit_size);

void *SMsafeRealloc(void *block, size_t size);

#endif