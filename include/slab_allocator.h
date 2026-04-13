#ifndef CRUX_LANG_SLAB_ALLOCATOR_H
#define CRUX_LANG_SLAB_ALLOCATOR_H
#include <stdint.h>
#include <stdbool.h>

typedef struct SlabNode SlabNode;
struct SlabNode {
    SlabNode *next;
    // Memory slots follow immediately in memory
};

typedef struct SlabAllocator {
    uint16_t slot_size;
    uint16_t capacity;
    void *free_list;      // Head of global free list
    SlabNode *slab_head;  // Track slabs for destruction
} SlabAllocator;

SlabAllocator *init_slab_allocator(uint16_t slot_size, uint16_t capacity);
void destroy_slab_allocator(SlabAllocator *allocator);
void *allocate_from_slab(SlabAllocator *allocator);
void free_from_slab(SlabAllocator *allocator, void *ptr);

#endif
