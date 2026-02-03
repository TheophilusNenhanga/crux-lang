#ifndef CRUX_LANG_SLAB_ALLOCATOR_H
#define CRUX_LANG_SLAB_ALLOCATOR_H
#include <stdint.h>
#include <stdbool.h>

#include "vm.h"

typedef struct SlabAllocator SlabAllocator;

struct SlabAllocator
{
    uint8_t* memory;
    int16_t* next_indexes;

    int16_t capacity;
    int16_t slot_size;
    int16_t free_head;
    int16_t used_count;

    SlabAllocator* next;
};

SlabAllocator * init_slab_allocator(uint16_t slot_size, uint16_t capacity);
void destroy_slab_allocator(SlabAllocator * slab_allocator);
bool is_slab_full(const SlabAllocator * slab_allocator);
void* allocate_from_slab(SlabAllocator * slab_allocator);
void free_from_slab(SlabAllocator * slab_allocator, void* ptr);

#define FREE_OBJECT(vm, type, pointer) free_slab_object((vm), (pointer), sizeof(type))

#endif // CRUX_LANG_SLAB_ALLOCATOR_H
