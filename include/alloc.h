#ifndef CRUX_LANG_ALLOC_H
#define CRUX_LANG_ALLOC_H

#include "vm.h"

void* alloc_memory(VM* vm, size_t size);
void free_memory(VM* vm, void* ptr, size_t curr_size);

#define FREE_OBJECT(vm, type, pointer) free_memory((vm), (pointer), sizeof(type))

#endif // CRUX_LANG_ALLOC_H
