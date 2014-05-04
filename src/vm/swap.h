#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>

void swap_init(void);
void swap_in(size_t swap_index, void *addr);
void swap_free(size_t swap_index);
size_t swap_out(void *addr);
#endif
