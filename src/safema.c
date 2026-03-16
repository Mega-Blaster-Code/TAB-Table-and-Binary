#include "safema.h"

void SMerror(int ec){
	exit(ec);
}

void *SMsafeMalloc(size_t size){
	void *ptr = malloc(size);
	if(ptr == NULL){
		SMerror(_SM_MALLOC_FAIL);
	}

	return ptr;
}

void *SMsafeCalloc(size_t size, size_t unit_size){
	void *ptr = calloc(size, unit_size);
	if(ptr == NULL){
		SMerror(_SM_CALLOC_FAIL);
	}

	return ptr;
}

void *SMsafeRealloc(void *block, size_t size){
	void *ptr = realloc(block, size);
	if(ptr == NULL){
		SMerror(_SM_REALLOC_FAIL);
	}
	return ptr;
}